//
// Created by Killian on 28/04/18.
//

#ifndef COEDIT_SYSTEM_CLIENT_HPP
#define COEDIT_SYSTEM_CLIENT_HPP

#include <experimental/filesystem>

#include "../core/basic_file_editor.hpp"
#include "../tui/curses_interface.hpp"


namespace coedit {
namespace system {


namespace stdfs = std::experimental::filesystem;


class client
{
public:
    using char_type = core::file_editor::char_type;
    
    using file_editor_type = core::file_editor;
    
    using file_editor_command_type = core::file_editor_command;
    
    using interface_type = tui::curses_interface;
    
    client(stdfs::path fle_path);
    
    int execute();

private:
    file_editor_type file_editr_;
    
    interface_type interf_;
};


}
}


#endif
