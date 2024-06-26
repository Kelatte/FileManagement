#include <iostream>
#include <sstream>
#include <string>

#include "fs.h"
#include "printfc.h"
#include "sys.h"

using namespace std;
void cmd();
void init();
FileManageMent* fileSystem = new FileManageMent();

int main() {
  init();
  cmd();
}

void init() {
  init_inode_table();
  mount_root();
  printfc(FG_YELLOW, string("系统时间为: ") + longtoTime(CurrentTime()));
}

void fresh_cmd() {
  printfc(FG_GREEN, "[Team1:CabbageDog] ");
  printfc(FG_BLUE, fileSystem->name);
  printfc(FG_WHITE, "> ");
}

void cmd() {
  fresh_cmd();
  string input, command, path, newPath;
  int i;
  while (getline(cin, input)) {
    istringstream is(input);
    string s;  // deal this inputs
    for (i = 0; is >> s; i++) {
      if (s.compare("exit") == 0) {  // command is exit
        cmd_exit();
        return;
      }
    }
    if (i != 1 & i != 2 & i != 3) {
      perrorc("your input is Illegal");
      fresh_cmd();
      continue;
    }
    istringstream temp(input);
    temp >> command >> path >> newPath;

    if (command.compare("ls") == 0) {
      const char* pa = path.c_str();
      int code = cmd_ls(pa);
      myhint(code);
    } else if (command.compare("cd") == 0) {
      const char* pa = path.c_str();
      int code = cmd_cd(pa);
      myhint(code);
    } else if (command.compare("mkdir") == 0) {
      const char* pa = path.c_str();
      int code = cmd_mkdir(pa, S_IFDIR);
      myhint(code);
    } else if (command.compare("touch") == 0) {
      const char* pa = path.c_str();
      int code = cmd_touch(pa, S_IFREG);
      myhint(code);
    } else if (command.compare("cat") == 0) {
      const char* pa = path.c_str();
      int code = cmd_cat(pa);
      myhint(code);
    } else if (command.compare("rmdir") == 0) {
      const char* pa = path.c_str();
      int code = cmd_rmdir(pa);
      myhint(code);
    } else if (command.compare("rm") == 0) {
      const char* pa = path.c_str();
      int code = cmd_rm(pa);
      myhint(code);
    } else if (command.compare("stat") == 0) {
      const char* pa = path.c_str();
      int code = cmd_stat(pa);
      myhint(code);
    } else if (command.compare("pwd") == 0) {
      int code = cmd_pwd();
      myhint(code);
    } else if (command.compare("vi") == 0) {
      const char* pa = path.c_str();
      int code = cmd_vi(pa);
      myhint(code);
    } else if (command.compare("dd") == 0) {
      string str = path + " " + newPath;
      const char* pa = str.c_str();
      int code = cmd_dd(pa);
      myhint(code);
    } else if (command.compare("init") == 0) {
      initialize_block(ROOT_DEV);
    } else {
      perrorc("your input is Illegal");
    }
    path = "";
    newPath = "";
    fresh_cmd();
  }
}
