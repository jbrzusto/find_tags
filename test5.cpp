#include <unordered_map>
#include <set>
#include <string>
#include <iostream>

struct Node {
  std::string name;
  int number;
};

struct Set { 
  std::set < int > s;
  size_t hash;
  Set() : hash(0) {};
  void insert (int x) { s.insert(x); hash %= x;};
  void erase (int x) { s.erase(x); hash %= x;};
};

struct hashSet {
  size_t operator() (const Set * x) const {
    return x->hash;
  };
};

struct setEqual {
  bool operator() ( const Set * x1, const Set * x2 ) const {
    return x1->hash == x2->hash && x1->s == x2->s;
  };
};
 
std::unordered_map < Set * , Node *, hashSet, setEqual > mapme;

main (int argc, char *argv[]) {
  Set k;
  k.insert(34);
  k.insert(53);
  Node n;
  n.name="testing";
  n.number=11;
  mapme.insert(std::make_pair(&k, &n));
  auto bb = mapme.find(& k);
  std::cout << bb->second->name << std::endl;
  Set k2 = k;
  k2.insert(44);
  Node n2;
  n2.name="second";
  n2.number=234234;
  mapme.insert(std::make_pair(&k2, &n2));
  std::cout << mapme.size() << std::endl;
  Set k3 = k2;
  auto cc = mapme.find(&k3);
  std::cout << cc->second->name << std::endl;
};
