#!/usr/bin/env python3
# Copyright (c) 2026-present The Block Zero developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the Block Zero Development & Growth Fund.

From the activation height on, every coinbase must pay at least
dev_fund_min_percent (10%) of the block subsidy to the fund script.
Miners contribute -devfundpercent (default 20%), clamped to the minimum.
"""

from decimal import Decimal

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal
from test_framework.wallet import MiniWallet

DEV_FUND_SCRIPT_HEX = "0014db3df23b245b8877f93a203c712597bd099a1144"
ACTIVATION_HEIGHT = 110
SUBSIDY = Decimal(50)  # regtest subsidy below the first halving (height 150)


class MiningDevFundTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.extra_args = [
            # Node 0 enforces and contributes the default 20%.
            [f"-testactivationheight=devfund@{ACTIVATION_HEIGHT}"],
            # Node 1 has the fund disabled and mines legacy coinbases.
            [],
        ]

    def fund_value_in_coinbase(self, node, block_hash):
        coinbase = node.getblock(block_hash, 2)["tx"][0]
        return sum(
            out["value"]
            for out in coinbase["vout"]
            if out["scriptPubKey"]["hex"] == DEV_FUND_SCRIPT_HEX
        )

    def run_test(self):
        node = self.nodes[0]
        legacy = self.nodes[1]
        wallet = MiniWallet(node)
        legacy_wallet = MiniWallet(legacy)

        self.log.info("Before activation: coinbase has no fund output")
        blocks = self.generate(wallet, ACTIVATION_HEIGHT - 2)  # heights 1..108
        assert_equal(self.fund_value_in_coinbase(node, blocks[-1]), 0)
        info = node.getmininginfo()
        assert_equal(info["devfund"]["active"], False)
        assert_equal(info["devfund"]["activation_height"], ACTIVATION_HEIGHT)
        assert_equal(info["devfund"]["min_percent"], 10)
        assert_equal(info["devfund"]["percent"], 20)
        assert "devfund" not in legacy.getmininginfo()

        block_109 = self.generate(wallet, 1)[0]
        assert_equal(self.fund_value_in_coinbase(node, block_109), 0)

        self.log.info("At activation: default contribution is 20% of the subsidy")
        block_110 = self.generate(wallet, 1)[0]
        assert_equal(self.fund_value_in_coinbase(node, block_110), SUBSIDY * 20 / 100)
        assert_equal(node.getmininginfo()["devfund"]["active"], True)
        # The legacy node accepts compliant blocks (no rule on its side).
        assert_equal(legacy.getblockcount(), ACTIVATION_HEIGHT)

        self.log.info("-devfundpercent below the consensus minimum is clamped to 10%")
        self.restart_node(0, extra_args=self.extra_args[0] + ["-devfundpercent=1"])
        self.connect_nodes(0, 1)
        assert_equal(node.getmininginfo()["devfund"]["percent"], 10)
        block_111 = self.generate(wallet, 1)[0]
        assert_equal(self.fund_value_in_coinbase(node, block_111), SUBSIDY * 10 / 100)

        self.log.info("Supported lower level: -devfundpercent=15")
        self.restart_node(0, extra_args=self.extra_args[0] + ["-devfundpercent=15"])
        self.connect_nodes(0, 1)
        block_112 = self.generate(wallet, 1)[0]
        assert_equal(self.fund_value_in_coinbase(node, block_112), SUBSIDY * 15 / 100)

        self.log.info("A coinbase without the fund output is rejected (bad-cb-devfund)")
        self.disconnect_nodes(0, 1)
        bad_block = self.generate(legacy_wallet, 1, sync_fun=self.no_op)[0]
        assert_equal(self.fund_value_in_coinbase(legacy, bad_block), 0)
        assert_equal(node.submitblock(legacy.getblock(bad_block, 0)), "bad-cb-devfund")
        assert_equal(node.getblockcount(), 112)


if __name__ == "__main__":
    MiningDevFundTest(__file__).main()
