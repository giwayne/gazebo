/*
 * Copyright (C) 2015-2016 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

#include "gazebo/gui/qgv/QGVNode.h"
#include "gazebo/gui/qgv/QGVEdge.h"

#include "gazebo/gui/qt.h"
#include "gazebo/gui/model/GraphScene.hh"
#include "gazebo/gui/model/GraphScene_TEST.hh"

#include "test_config.h"

using namespace gazebo;
using namespace gui;

/////////////////////////////////////////////////
void GraphScene_TEST::Initialization()
{
  GraphScene *gs = new GraphScene();
  QVERIFY(gs != nullptr);
  delete gs;
}

/////////////////////////////////////////////////
void GraphScene_TEST::NodeUpdates()
{
  GraphScene gs;
  QVERIFY(!gs.HasNode("node1"));
  QVERIFY(gs.GetNode("node1") == nullptr);

  QGVNode *node1 = gs.AddNode("node1");

  QVERIFY(node1 != nullptr);
  QVERIFY(gs.HasNode("node1"));
  QVERIFY(gs.GetNode("node1") != nullptr);

  gs.RemoveNode("node1");

  QVERIFY(!gs.HasNode("node1"));
  QVERIFY(gs.GetNode("node1") == nullptr);
}

/////////////////////////////////////////////////
void GraphScene_TEST::EdgeUpdates()
{
  GraphScene gs;
  QGVNode *node1 = nullptr;
  QGVNode *node2 = nullptr;
  QGVEdge *edge1 = nullptr;
  QGVEdge *edgeBad = nullptr;

  edgeBad = gs.AddEdge("edgeBad", "nodeBad", "nodeBad2");
  QVERIFY(edgeBad == nullptr);

  node1 = gs.AddNode("node1");
  QVERIFY(node1 != nullptr);

  node2 = gs.AddNode("node2");
  QVERIFY(node2 != nullptr);

  edge1 = gs.AddEdge("edge1", "node1", "node2");
  QVERIFY(edge1 != nullptr);

  QVERIFY(gs.hasEdge(tr("edge1")));

  gs.RemoveEdge("edge1");

  QVERIFY(!gs.hasEdge(tr("edge1")));
  QVERIFY(gs.HasNode("node1"));
  QVERIFY(gs.HasNode("node2"));
}

/////////////////////////////////////////////////
void GraphScene_TEST::EdgeColor()
{
  GraphScene gs;
  QGVEdge *edge1 = nullptr;

  gs.AddNode("node1");
  gs.AddNode("node2");

  edge1 = gs.AddEdge("edge1", "node1", "node2");

  common::Color c1(1, 0, 0);
  gs.SetEdgeColor("edge1", c1);

  QColor c2 = edge1->color();

  QCOMPARE(c2.red(), 255);
  QCOMPARE(c2.green(), 0);
  QCOMPARE(c2.blue(), 0);
}

// Generate a main function for the test
QTEST_MAIN(GraphScene_TEST)
