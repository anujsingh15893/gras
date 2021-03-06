# Copyright (C) by Josh Blum. See LICENSE.txt for licensing information.

import unittest
import gras
import numpy
from demo_blocks import *
import json

class QueryTest(unittest.TestCase):

    def setUp(self):
        self.tb = gras.TopBlock()

    def tearDown(self):
        self.tb = None

    def test_simple(self):
        vec_source = VectorSource(numpy.uint32, [0, 9, 8, 7, 6])
        vec_sink = VectorSink(numpy.uint32)

        self.tb.connect(vec_source, vec_sink)
        self.tb.run()

        self.assertEqual(vec_sink.get_vector(), (0, 9, 8, 7, 6))

        blocks_json = self.tb.query("<args><path>/blocks.json</path></args>")
        print blocks_json
        json.loads(blocks_json)

        stats_json = self.tb.query("<args><path>/stats.json</path></args>")
        print stats_json
        json.loads(stats_json)

if __name__ == '__main__':
    unittest.main()
