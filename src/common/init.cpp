// Copyright (c) 2023-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <common/args.h>
#include <common/init.h>
#include <logging.h>
#include <tinyformat.h>
#include <util/chaintype.h>
#include <util/fs.h>
#include <util/readwritefile.h>
#include <util/translation.h>

#include <algorithm>
#include <exception>
#include <optional>
#include <sstream>
#include <string>

namespace {
//! Create a mainnet bitcoin.conf on first run so the GUI works without manual setup.
void EnsureBlockZeroDefaultConfigFile(const ArgsManager& args, const fs::path& datadir_path, const fs::path& config_path)
{
    if (args.IsArgSet("-datadir")) return;
    if (config_path.empty()) return;
    const fs::path expected_config = datadir_path / BITCOIN_CONF_FILENAME;
    // Only auto-create when the effective config path is the default
    // datadir/bitcoin.conf. Compare lexically rather than with fs::equivalent():
    // equivalent() requires both files to already exist and otherwise throws
    // (on macOS: "filesystem error: in equivalent: Operation not supported"),
    // which is exactly the first-run case this function needs to handle.
    if (config_path.lexically_normal() != expected_config.lexically_normal()) return;
    if (args.GetChainType() != ChainType::MAIN) return;
    if (fs::exists(expected_config)) return;

    fs::create_directories(datadir_path);
    // Note: intentionally no txindex here. The GUI first-run dialog lets users
    // pick "limit block chain storage" (prune), and txindex=1 is incompatible
    // with prune ("Prune mode is incompatible with -txindex"), which would crash
    // the wallet on launch. A desktop wallet does not need txindex; the server /
    // explorer configs set it separately where pruning is off.
    const std::string contents =
        "# Block Zero mainnet (auto-created)\n"
        "server=1\n"
        "\n"
        "[main]\n"
        "listen=1\n"
        "rpcbind=127.0.0.1\n"
        "rpcallowip=127.0.0.1\n"
        "rpcport=8332\n"
        "addnode=217.160.46.61:8210\n";
    if (WriteBinaryFile(expected_config, contents)) {
        LogInfo("Created default config at %s", fs::PathToString(expected_config));
    } else {
        LogWarning("Could not write default config to %s", fs::PathToString(expected_config));
    }
}

//! Comment out a stale `txindex` setting in the default desktop mainnet config.
//! Older Block Zero builds auto-created bitcoin.conf with `txindex=1`. Combined
//! with the GUI "limit block chain storage" (prune) choice this aborts startup
//! with "Prune mode is incompatible with -txindex" — so an updated wallet still
//! fails for users who kept their old datadir. Neutralize it automatically so
//! non-technical users never have to hand-edit a config file.
//!
//! Guarded to the *default* datadir/bitcoin.conf on mainnet only: it never
//! touches server/explorer configs (those pass -datadir and legitimately use
//! txindex with pruning disabled).
void SanitizeBlockZeroStaleTxindex(const ArgsManager& args, const fs::path& datadir_path, const fs::path& config_path)
{
    if (args.IsArgSet("-datadir")) return;
    if (config_path.empty()) return;
    const fs::path expected_config = datadir_path / BITCOIN_CONF_FILENAME;
    if (config_path.lexically_normal() != expected_config.lexically_normal()) return;
    if (args.GetChainType() != ChainType::MAIN) return;
    if (!fs::exists(expected_config)) return;

    const auto [read_ok, contents] = ReadBinaryFile(expected_config);
    if (!read_ok) return;

    std::istringstream stream{contents};
    std::string line;
    std::string out;
    bool changed = false;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back(); // normalize CRLF
        const size_t first = line.find_first_not_of(" \t");
        if (first != std::string::npos && line.compare(first, 7, "txindex") == 0) {
            // Active (uncommented) txindex line: `txindex` optionally followed by `=value`.
            const char after = (first + 7 < line.size()) ? line[first + 7] : '\0';
            if (after == '\0' || after == '=' || after == ' ' || after == '\t') {
                out += "# " + line + "    # auto-disabled: incompatible with prune (Block Zero)\n";
                changed = true;
                continue;
            }
        }
        out += line;
        out += "\n";
    }

    if (changed) {
        if (WriteBinaryFile(expected_config, out)) {
            LogInfo("Disabled stale txindex in %s (incompatible with prune)", fs::PathToString(expected_config));
        } else {
            LogWarning("Could not rewrite %s to disable stale txindex", fs::PathToString(expected_config));
        }
    }
}
} // namespace

namespace common {
std::optional<ConfigError> InitConfig(ArgsManager& args, SettingsAbortFn settings_abort_fn)
{
    try {
        if (!CheckDataDirOption(args)) {
            return ConfigError{ConfigStatus::FAILED, strprintf(_("Specified data directory \"%s\" does not exist."), args.GetArg("-datadir", ""))};
        }

        // Record original datadir and config paths before parsing the config
        // file. It is possible for the config file to contain a datadir= line
        // that changes the datadir path after it is parsed. This is useful for
        // CLI tools to let them use a different data storage location without
        // needing to pass it every time on the command line. (It is not
        // possible for the config file to cause another configuration to be
        // used, though. Specifying a conf= option in the config file causes a
        // parse error, and specifying a datadir= location containing another
        // bitcoin.conf file just ignores the other file.)
        const fs::path orig_datadir_path{args.GetDataDirBase()};
        const fs::path orig_config_path{AbsPathForConfigVal(args, args.GetPathArg("-conf", BITCOIN_CONF_FILENAME), /*net_specific=*/false)};

        EnsureBlockZeroDefaultConfigFile(args, orig_datadir_path, orig_config_path);
        SanitizeBlockZeroStaleTxindex(args, orig_datadir_path, orig_config_path);

        std::string error;
        if (!args.ReadConfigFiles(error, true)) {
            return ConfigError{ConfigStatus::FAILED, strprintf(_("Error reading configuration file: %s"), error)};
        }

        // Check for chain settings (Params() calls are only valid after this clause)
        SelectParams(args.GetChainType());

        // Create datadir if it does not exist.
        const auto base_path{args.GetDataDirBase()};
        if (!fs::exists(base_path)) {
            // When creating a *new* datadir, also create a "wallets" subdirectory,
            // whether or not the wallet is enabled now, so if the wallet is enabled
            // in the future, it will use the "wallets" subdirectory for creating
            // and listing wallets, rather than the top-level directory where
            // wallets could be mixed up with other files. For backwards
            // compatibility, wallet code will use the "wallets" subdirectory only
            // if it already exists, but never create it itself. There is discussion
            // in https://github.com/bitcoin/bitcoin/issues/16220 about ways to
            // change wallet code so it would no longer be necessary to create
            // "wallets" subdirectories here.
            fs::create_directories(base_path / "wallets");
        }
        const auto net_path{args.GetDataDirNet()};
        if (!fs::exists(net_path)) {
            fs::create_directories(net_path / "wallets");
        }

        // Show an error or warn/log if there is a bitcoin.conf file in the
        // datadir that is being ignored.
        const fs::path base_config_path = base_path / BITCOIN_CONF_FILENAME;
        if (fs::exists(base_config_path)) {
            if (orig_config_path.empty()) {
                LogInfo(
                    "Data directory %s contains a %s file which is explicitly ignored using -noconf.",
                    fs::quoted(fs::PathToString(base_path)),
                    fs::quoted(BITCOIN_CONF_FILENAME));
            } else if (!fs::equivalent(orig_config_path, base_config_path)) {
                const std::string cli_config_path = args.GetArg("-conf", "");
                const std::string config_source = cli_config_path.empty()
                    ? strprintf("data directory %s", fs::quoted(fs::PathToString(orig_datadir_path)))
                    : strprintf("command line argument %s", fs::quoted("-conf=" + cli_config_path));
                std::string error = strprintf(
                    "Data directory %1$s contains a %2$s file which is ignored, because a different configuration file "
                    "%3$s from %4$s is being used instead. Possible ways to address this would be to:\n"
                    "- Delete or rename the %2$s file in data directory %1$s.\n"
                    "- Change datadir= or conf= options to specify one configuration file, not two, and use "
                    "includeconf= to include any other configuration files.",
                    fs::quoted(fs::PathToString(base_path)),
                    fs::quoted(BITCOIN_CONF_FILENAME),
                    fs::quoted(fs::PathToString(orig_config_path)),
                    config_source);
                if (args.GetBoolArg("-allowignoredconf", false)) {
                    LogWarning("%s", error);
                } else {
                    error += "\n- Set allowignoredconf=1 option to treat this condition as a warning, not an error.";
                    return ConfigError{ConfigStatus::FAILED, Untranslated(error)};
                }
            }
        }

        // Create settings.json if -nosettings was not specified.
        if (args.GetSettingsPath()) {
            std::vector<std::string> details;
            if (!args.ReadSettingsFile(&details)) {
                const bilingual_str& message = _("Settings file could not be read");
                if (!settings_abort_fn) {
                    return ConfigError{ConfigStatus::FAILED, message, details};
                } else if (settings_abort_fn(message, details)) {
                    return ConfigError{ConfigStatus::ABORTED, message, details};
                } else {
                    details.clear(); // User chose to ignore the error and proceed.
                }
            }
            if (!args.WriteSettingsFile(&details)) {
                const bilingual_str& message = _("Settings file could not be written");
                return ConfigError{ConfigStatus::FAILED_WRITE, message, details};
            }
        }
    } catch (const std::exception& e) {
        return ConfigError{ConfigStatus::FAILED, Untranslated(e.what())};
    }
    return {};
}
} // namespace common
