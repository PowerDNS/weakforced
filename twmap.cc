#include "twmap.hh"
#include <iostream>
#include <unistd.h>

TWStatsTypeMap g_field_types{};

std::mutex dbMap_mutx;
std::map<std::string, TWStringStatsDBWrapper> dbMap;

#ifdef MAIN
#include <string.h>

int main (int argc, char** argv)
{
  FieldMap field_map;

  std::cout << std::lround(3.9) << "\n";

  field_map[std::string("numWidgets")] = std::string("int");
  field_map[std::string("diffWidgets")] = std::string("hll");
  field_map[std::string("numWidgetTypes")] = std::string("countmin");

  auto sw = TWStringStatsDBWrapper(1, 60);
  sw.setFields(field_map);

  // test integers
  for (int i=0; i<10; i++) {
    sw.sdbp->add("neil.cook", "numWidgets", 1);
    sw.sdbp->add("neil.cook", "numWidgets", 1);
    int k = sw.sdbp->sub("neil.cook", "numWidgets", 1);
    sleep(1);
    // std::cout << k << "\n";
  }
  std::cout << "get_current is " << sw.sdbp->get_current("neil.cook", "numWidgets") << "\n";
  std::cout << "get is " << sw.sdbp->get("neil.cook", "numWidgets") << "\n";
  std::vector<int> vec;
  sw.sdbp->get_windows("neil.cook", "numWidgets", vec);
  for (auto i : vec) {
    std::cout << "window has value " << i << "\n";
  }  

  // test hyperloglog
  for (int i=0; i<10; i++) {
    sw.sdbp->add("neil.cook", "diffWidgets", "widget1");
    sw.sdbp->add("neil.cook", "diffWidgets", "widget2");
    sw.sdbp->add("neil.cook", "diffWidgets", "widget3");
    sw.sdbp->add("neil.cook", "diffWidgets", "widget4");
    sleep(1);
    // std::cout << k << "\n";
  }
  std::cout << "get_current is " << sw.sdbp->get_current("neil.cook", "diffWidgets") << "\n";
  std::cout << "get is " << sw.sdbp->get("neil.cook", "diffWidgets") << "\n";
  vec.clear();
  sw.sdbp->get_windows("neil.cook", "diffWidgets", vec);
  for (auto i : vec) {
    std::cout << "window has value " << i << "\n";
  }  

  // test countmin
  for (int i=0; i<10; i++) {
    sw.sdbp->add("neil.cook", "numWidgetTypes", "widget1", 20);
    sw.sdbp->add("neil.cook", "numWidgetTypes", "widget2", 40);
    sw.sdbp->add("neil.cook", "numWidgetTypes", "widget3", 60);
    sleep(1);
    // std::cout << k << "\n";
  }
  std::cout << "get_current is " << sw.sdbp->get_current("neil.cook", "numWidgetTypes") << "\n";
  std::cout << "get is " << sw.sdbp->get("neil.cook", "numWidgetTypes") << "\n";
  std::cout << "get widget1 is " << sw.sdbp->get("neil.cook", "numWidgetTypes", "widget1") << "\n";
  std::cout << "get widget2 is " << sw.sdbp->get("neil.cook", "numWidgetTypes", "widget2") << "\n";
  std::cout << "get widget2 is " << sw.sdbp->get("neil.cook", "numWidgetTypes", "widget3") << "\n";
  std::cout << "get nonExistent is " << sw.sdbp->get("neil.cook", "numWidgetTypes", "nonExistent") << "\n";
  std::cout << "get nonExistent key is " << sw.sdbp->get("dddneil.cook", "numWidgetTypes", "nonExistent") << "\n";
  std::cout << "get nonExistent field is " << sw.sdbp->get("neil.cook", "dddnumWidgetTypes", "nonExistent") << "\n";
  vec.clear();
  sw.sdbp->get_windows("neil.cook", "numWidgetTypes", vec);
  for (auto i : vec) {
    std::cout << "window has value " << i << "\n";
  }  


}

#endif
