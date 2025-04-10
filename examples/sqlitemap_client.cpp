// sqlitemap - client
// SPDX-FileCopyrightText: 2024-present Benno Waldhauer
// SPDX-License-Identifier: MIT

#include <codecvt>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <bw/sqlitemap/sqlitemap.hpp>

using namespace bw::sqlitemap;

const char* app_title = "sqlitemap";

class require_client_termination : public std::runtime_error
{
  public:
    require_client_termination()
        : std::runtime_error("SQLITEMAP_CLIENT_TERMINATION_REQUIRED")
    {
    }
};

void show_title()
{
    std::cout << app_title << " - Simple key-value store based on SQLite" << std::endl;
}

void show_help_hint()
{
    std::cout << "Enter '?' for usage hints" << std::endl;
}

void show_prompt_commands()
{
    std::cout
        << "Prompt commands:\n"
        << "  ?,  help                 Show help\n"
        << "  g,  get <key>            Retrieve value for key\n"
        << "  p,  put <key> <value>    Store key-value pair\n"
        << "  d,  del <key>            Delete value for key\n"
        << "  ls, list                 List all key-value pairs\n"
        << "      size                 Show number of key-value pairs\n"
        << "  t,  table                Show current table\n"
        << "  ts, tables               List available tables\n"
        << "  #   select <table|#>     Switch to a different table.\n"
        << "  f,  file                 Show file\n"
        << "  m,  mode                 Show mode\n"
        << "  tr, transaction          Start transaction\n"
        << "  c,  commit               Commit\n"
        << "  r,  rollback             Rollback\n"
        << "      clear                Clear current table. USE WITH CARE\n"
        << "      delete_db            Delete this database file. USE WITH CARE\n"
        << "      layout <flag> <arg>  Define the displayed column width\n"
        << "                           e.g. k 10 => sets the column with for keys to 10 chars\n"
        << "                                v 80 => sets the column with for values to 80 chars\n"
        << "      [!]auto_refresh      Toggle auto list refresh\n"
        << "      cls                  Clear screen\n"
        << "  q,  quit                 Quit program\n";
}

void show_usage()
{
    std::cout << "Usage:\n"
              << "  sqlitemap -f <file> -t <table> -[rwcnax]*\n"
              << "  sqlitemap <file> <table> -[rwcnax]*\n"
              << "\n"
              << "  e.g. sqlitemap ./test.db logs -ca\n"
              << "  Creates table 'logs' if not existing in \n"
              << "  'test.db' located in working dir. has auto commit enabled\n"
              << "\n"
              << "Command line options:\n"
              << "  -f <file>       SQLite filename\n"
              << "  -t <table>      Table name\n"
              << "  -c, -r, -w, -n  Operation modes:\n"
              << "                  c - default, open for r/w, creating db/table\n"
              << "                  r - open as ready-only\n"
              << "                  w - open for r/w, but drop <table> contents first\n"
              << "                  n - create new database (erasing existing tables!)\n"
              << "  -a, -x          Auto-commit (a=on, x=off)\n"
              << "  -v              Verbose, enabled sqlitemap logging\n"
              << "  --help          Show this help message\n";
}

void show_help()
{
    show_title();
    std::cout << std::endl;
    show_usage();
    std::cout << std::endl;
    show_prompt_commands();
}

void clear_screen()
{
    std::cout << "\033[2J\033[H"; // ANSI escape code to clear screen
}

std::string mode_to_string(bw::sqlitemap::operation_mode mode)
{
    switch (mode)
    {
    case bw::sqlitemap::operation_mode::r:
        return "r";
    case bw::sqlitemap::operation_mode::w:
        return "w";
    case bw::sqlitemap::operation_mode::c:
        return "c";
    case bw::sqlitemap::operation_mode::n:
        return "n";
    default:
        return "unknown mode";
    }
}

std::string mode_to_detailed_string(bw::sqlitemap::operation_mode mode)
{
    std::string m = mode_to_string(mode);

    switch (mode)
    {
    case bw::sqlitemap::operation_mode::r:

    default:
        return m;
    }
}

std::wstring utf8_to_wstring(const std::string& str)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
    return converter.from_bytes(str);
}

std::string wstring_to_utf8(const std::wstring& wstr)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
    return converter.to_bytes(wstr);
}

class sqlitemap_client
{
  private:
    std::unique_ptr<sqlitemap<>> sm;
    bool auto_list_refresh = false;
    std::map<std::string, std::string> tables;

    struct layout
    {
        int max_key_length = 24;
        int max_value_length = 40;
        int min_col_size = 5;
        int rowid_space = 5;
        std::string column_separator = "│";
        std::string row_separator = "─";
        std::string padding = " ";

        std::string render_top_border() const
        {
            wchar_t row_sep = utf8_to_wstring(row_separator).at(0);
            std::wstring bar(2 + max_key_length + 3 + max_value_length + 2, row_sep);

            bar[0] = L'┌';
            bar[2 + max_key_length + 1] = L'┬';
            bar[bar.size() - 1] = L'┐';

            std::wstring rowid_border(rowid_space + 1, ' ');

            return wstring_to_utf8(rowid_border + bar);
        }

        std::string render_bottom_border() const
        {
            wchar_t row_sep = utf8_to_wstring(row_separator).at(0);
            std::wstring bar(2 + max_key_length + 3 + max_value_length + 2, row_sep);

            bar[0] = L'└';
            bar[2 + max_key_length + 1] = L'┴';
            bar[bar.size() - 1] = L'┘';

            std::wstring rowid_border(rowid_space + 1, ' ');

            return wstring_to_utf8(rowid_border + bar);
        }

        std::string render_row(const std::string& key, const std::string& value,
                               std::optional<std::string> rowid = std::nullopt) const
        {
            std::ostringstream oss;
            oss << std::setw(rowid_space) << std::right << rowid.value_or("") << padding;
            oss << column_separator << padding;
            oss << render_element(key, max_key_length);
            oss << padding << column_separator << padding;
            oss << render_element(value, max_value_length);
            oss << padding << column_separator;
            return oss.str();
        }

        std::string render_element(const std::string& element_u8, int max_size) const
        {
            std::wstring abbr = L"...";
            std::wstring element = utf8_to_wstring(element_u8);

            if (element.size() > max_size && max_size && max_size > abbr.size())
            {
                return wstring_to_utf8(element.substr(0, max_size - abbr.size()) + abbr);
            }

            int num_spaces_to_add = max_size - element.size();
            return wstring_to_utf8(element.append(num_spaces_to_add, L' '));
        }
    } layout;

  public:
    sqlitemap_client(const std::string& file, const std::string& table, operation_mode mode,
                     bool auto_commit, log_level log_level)
        : sm(std::make_unique<sqlitemap<>>(file, table, mode, auto_commit, log_level))
    {
    }

    void show_prompt()
    {
        std::cout << std::endl << app_title << "|" << sm->config().table() << "> ";
    }

    void index_tables()
    {
        try
        {
            tables.clear();

            std::vector<std::string> table_list = get_tablenames(sm->config().filename());
            std::sort(table_list.begin(), table_list.end());

            int index_counter = 0;
            for (auto& t : table_list)
            {
                index_counter++;
                tables[std::to_string(index_counter)] = t;
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    std::string find_table_candidate(const std::string& table_request)
    {
        // name given
        auto it_in_names = std::find_if(tables.begin(), tables.end(),
                                        [&](const auto& e) { return e.second == table_request; });
        if (it_in_names != tables.end())
            return table_request;

        // select table by index
        auto it_in_index = tables.find(table_request);
        if (it_in_index != tables.end())
            return it_in_index->second;

        // will create new table on corresponding mode
        return table_request;
    }

    void select_table(const std::string& table_request)
    {
        std::string table = find_table_candidate(table_request);

        std::string file = sm->config().filename();
        operation_mode om = sm->config().mode();
        bool ac = sm->config().auto_commit();
        log_level ll = sm->config().log_level();

        sm = nullptr;
        sm = std::make_unique<sqlitemap<>>(file, table, om, ac, ll);
        std::cout << "Switched to table: " << table << std::endl;

        index_tables();

        if (auto_list_refresh)
        {
            list();
        }
    }

    void set_auto_list_refresh(bool enable)
    {
        bool changed = auto_list_refresh != enable;
        auto_list_refresh = enable;
        std::cout << "Automatic list refresh " << (enable ? "enabled" : "disabled") << std::endl;

        if (changed && enable)
        {
            watch_file();
        }
    }

    void get(const std::string& key)
    {
        try
        {
            auto opt_value = sm->try_get(key);
            std::cout << key << " -> " << opt_value.value_or("[not found]") << std::endl;
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    void del(const std::string& key)
    {
        try
        {
            sm->del(key);
            std::cout << key << " deleted" << std::endl;

            if (auto_list_refresh)
            {
                list();
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    void put(const std::string& key, const std::string& value)
    {
        try
        {
            sm->set(key, value);
            get(key);

            if (auto_list_refresh)
            {
                list();
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    void list()
    {
        if (auto_list_refresh)
        {
            clear_screen();
        }

        std::cout << layout.render_top_border() << std::endl;
        int row_counter = 0;
        for (const auto& [key, value] : *sm)
        {
            row_counter++;
            std::cout << layout.render_row(key, value, std::to_string(row_counter)) << std::endl;
        }
        std::cout << layout.render_bottom_border() << std::endl;
    }

    void transaction()
    {
        try
        {
            sm->begin_transaction();
            std::cout << "begin transaction" << std::endl;
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    void commit()
    {
        try
        {
            sm->commit();
            std::cout << "commit" << std::endl;
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    void rollback()
    {
        try
        {
            sm->rollback();
            std::cout << "rollback" << std::endl;
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    void size()
    {
        std::cout << sm->size() << std::endl;
    }

    void show_table()
    {
        std::cout << sm->config().table() << std::endl;
    }

    void show_tables()
    {
        index_tables();
        for (auto [i, t] : tables)
        {
            std::cout << std::setw(layout.rowid_space) << std::right << i << layout.padding
                      << layout.column_separator << layout.padding << t << std::endl;
        }
    }

    void show_file()
    {
        std::cout << sm->config().filename() << std::endl;
    }

    void show_mode()
    {
        std::cout << "mode: " << mode_to_detailed_string(sm->config().mode())
                  << ", auto_commit: " << std::boolalpha << sm->config().auto_commit()
                  << std::noboolalpha << std::endl;
    }

    void clear()
    {
        try
        {
            std::string t = sm->config().table();
            std::cout << "Clear table '" << t << "'" << std::endl;
            sm->clear();
        }
        catch (const sqlitemap_error& e)
        {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    void delete_db()
    {
        try
        {
            std::string db = sm->config().filename();
            std::cout << "Try to delete database file '" << db << "'" << std::endl;
            sm->terminate();

            throw require_client_termination();
        }
        catch (const sqlitemap_error& e)
        {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    void update_layout(const std::string& flag, const std::string& arg)
    {
        std::cout << "LAYOUT " << flag << " " << arg << std::endl;

        try
        {
            if (flag == "k" || flag == "v")
            {
                int new_size = std::max(layout.min_col_size, std::atoi(arg.c_str()));
                if (flag == "k")
                    layout.max_key_length = new_size;
                else if (flag == "v")
                    layout.max_value_length = new_size;
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "Failed to update layout. Error: " << e.what() << std::endl;
        }

        if (auto_list_refresh)
        {
            list();
        }
        else
        {
            std::cout << layout.render_top_border() << std::endl;

            std::cout << layout.render_row("key-1", "value-a") << std::endl;
            std::cout << layout.render_row("key-2", "value-b") << std::endl;
            std::cout << layout.render_row("key-3", "value-c", "1") << std::endl;
            std::cout << layout.render_row("key-4", "value-d", "42") << std::endl;
            std::cout << layout.render_row("key-5", "value-e", "500") << std::endl;

            std::cout << layout.render_bottom_border() << std::endl;
        }
    }

    void watch_file()
    {
        namespace fs = std::filesystem;
        std::thread file_watcher(
            [&]
            {
                fs::path path = sm->config().filename();
                if (!fs::exists(path))
                {
                    std::cerr << "File to watch does not exist: " << path << std::endl;
                    return;
                }

                auto last_write_time = fs::last_write_time(path);

                while (auto_list_refresh)
                {
                    std::this_thread::sleep_for(std::chrono::seconds(1));

                    auto current_write_time = fs::last_write_time(path);
                    if (current_write_time != last_write_time)
                    {
                        list();
                        last_write_time = current_write_time;
                    }
                }
            });
        file_watcher.detach();
    }

    void run()
    {
        index_tables();

        std::string command;
        while (true)
        {
            show_prompt();

            if (!std::getline(std::cin, command)) // Check if input stream fails (EOF)
            {
                std::cout << std::endl;
                command = "exit";
            }

            if (command == "exit" || command == "quit" || command == "q")
            {
                std::cout << "Exiting..." << std::endl;
                break;
            }

            std::istringstream iss(command);
            std::string cmd, arg1, arg2;
            iss >> cmd >> arg1;
            std::getline(iss, arg2);
            if (!arg2.empty() && arg2[0] == ' ')
                arg2.erase(0, 1); // Trim leading space

            if (cmd == "#" || cmd == "select")
            {
                select_table(arg1);
            }
            else if (cmd == "g" || cmd == "get")
            {
                get(arg1);
            }
            else if (cmd == "p" || cmd == "put")
            {
                put(arg1, arg2);
            }
            else if (cmd == "d" || cmd == "del")
            {
                del(arg1);
            }
            else if (cmd == "t" || cmd == "table")
            {
                show_table();
            }
            else if (cmd == "f" || cmd == "file")
            {
                show_file();
            }
            else if (cmd == "ts" || cmd == "tables")
            {
                show_tables();
            }
            else if (cmd == "ls" || cmd == "list")
            {
                list();
            }
            else if (cmd == "tr" || cmd == "transaction")
            {
                transaction();
            }
            else if (cmd == "c" || cmd == "commit")
            {
                commit();
            }
            else if (cmd == "r" || cmd == "rollback")
            {
                rollback();
            }
            else if (cmd == "size")
            {
                size();
            }
            else if (cmd == "?" || cmd == "help")
            {
                show_prompt_commands();
            }
            else if (cmd == "m" || cmd == "mode")
            {
                show_mode();
            }
            else if (cmd == "clear")
            {
                clear();
            }
            else if (cmd == "delete_db")
            {
                delete_db();
            }
            else if (cmd == "layout")
            {
                update_layout(arg1, arg2);
            }
            else if (cmd == "auto_refresh")
            {
                set_auto_list_refresh(true);
            }
            else if (cmd == "!auto_refresh")
            {
                set_auto_list_refresh(false);
            }
            else if (cmd == "cls")
            {
                clear_screen();
            }
            else
            {
                std::cerr << "Unknown command" << std::endl;
            }
        }
    }
};

int main(int argc, char* argv[])
{
#ifdef _WIN32
    // enabled UTF-8 codepage for Windows
    // unix systems typically support UTF-8 by default
    system("chcp 65001 >nul");
#endif

    if (argc == 2 && std::string(argv[1]) == "--help")
    {
        show_help();
        return 0;
    }

    show_title();
    show_help_hint();

    std::string filename = default_filename;
    std::string table = default_table;
    operation_mode mode = default_mode;
    bool auto_commit = default_auto_commit;
    log_level log_level = log_level::off;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        if (arg == "-f" && i + 1 < argc)
        {
            filename = argv[++i];
        }
        else if (arg == "-t" && i + 1 < argc)
        {
            table = argv[++i];
        }
        else if (arg[0] == '-' && arg.size() > 1)
        {
            for (size_t f = 1; f < arg.size(); ++f)
            {
                switch (arg[f])
                {
                case 'r':
                    mode = operation_mode::r;
                    break;
                case 'w':
                    mode = operation_mode::w;
                    break;
                case 'c':
                    mode = operation_mode::c;
                    break;
                case 'n':
                    mode = operation_mode::n;
                    break;
                case 'a':
                    auto_commit = true;
                    break;
                case 'x':
                    auto_commit = false;
                    break;
                case 'v':
                    log_level = log_level::debug;
                    break;
                default:
                    std::cerr << "Unknown flag: -" << arg[f] << "\n";
                    return 1;
                }
            }
        }
        else if (filename == default_filename)
        {
            filename = arg;
        }
        else if (table == default_table)
        {
            table = arg;
        }
        else
        {
            std::cerr << "Unexpected argument: " << arg << "\n";
            return 1;
        }
    }

    try
    {
        sqlitemap_client client(filename, table, mode, auto_commit, log_level);
        client.run();
    }
    catch (const require_client_termination& e)
    {
        return 0; // was requested by application
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        ;
        return 1;
    }

    return 0;
}
