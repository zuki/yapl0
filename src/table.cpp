#include "log.hpp"
#include "table.hpp"

void SymTable::blockOut() {
  while (!symbolTable.empty() && symbolTable.back().level == cur_level) {
    symbolTable.pop_back();
  }
  cur_level--;
}

bool SymTable::findSymbol(const std::string &name, const NameType &type, const bool &checkLevel, int num) const {
  auto result = std::find_if(symbolTable.crbegin(), symbolTable.crend(),
  [&](const SymInfo &e) {
    auto cond = e.name == name && e.type == type;
    if (num != -1) cond = cond && e.num == num;
    if (checkLevel) cond = cond && e.level == cur_level;
    return cond;
  });
  return result == symbolTable.crend() ? false : true;
}

void SymTable::deleteTemp(const std::string &name) {
  auto it = std::find(tempNames.begin(), tempNames.end(), name);
  if (it != tempNames.end())
    tempNames.erase(it);
}

bool SymTable::findTemp(const std::string &name) const{
  auto it = std::find(tempNames.cbegin(), tempNames.cend(), name);
  return it == tempNames.cend() ? false : true;
}

void SymTable::dumpSymbolTable() const {
  for (const SymInfo &e : symbolTable)
    fprintf(stderr, "[%d] %-10s %s\n", e.level, e.name.c_str(), NameTypeStr(e.type).c_str());
}

void SymTable::dumpTempNames() const {
  fprintf(stderr, "remain symbols:");
  for (const std::string &name : tempNames)
    fprintf(stderr, " %s", name.c_str());
  fprintf(stderr, "\n");
}

const CodeInfo &CodeTable::find(const std::string &name) const {
  auto itr =
      std::find_if(infos.rbegin(), infos.rend(),
                   [&](const CodeInfo &info) { return info.name == name; });
  if (itr == infos.rend()) {
    Log::error((name +" is undefined").c_str(), true);
  }

  return *itr;
}

void CodeTable::leaveBlock() {
  while (!infos.empty() && infos.back().level == cur_level) {
    infos.pop_back();
  }
  cur_level--;
}

void CodeTable::dumpInfos() const {
  for (const CodeInfo &info : infos)
    fprintf(stderr, "[%d] %-10s %s\n", info.level, info.name.c_str(), NameTypeStr(info.type).c_str());
}
