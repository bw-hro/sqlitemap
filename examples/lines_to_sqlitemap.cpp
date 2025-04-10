// lines2sqlitemap
// SPDX-FileCopyrightText: 2024-present Benno Waldhauer
// SPDX-License-Identifier: MIT

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <regex>
#include <string>

#include <bw/sqlitemap/sqlitemap.hpp>

namespace sm = bw::sqlitemap;

const char* app_title = "lines2sqlitemap";

class processor
{
    using key_type = long long;
    using value_type = std::string;
    using DB = sm::sqlitemap<decltype(sm::config<key_type, value_type>().codecs())>;

  public:
    processor(const std::string& file, const std::string& table, bool echo = true)
        : db(sm::config<key_type, value_type>()
                 .filename(file)
                 .table(table)
                 .mode(sm::operation_mode::w)
                 .auto_commit(false))
        , line_count(0)
        , echo(echo)
    {
    }

    void process_input(const std::string& line)
    {
        long long count = line_count.fetch_add(1, std::memory_order_relaxed);
        long long timestamp = get_current_timestamp();

        static const std::regex ansi_escape("\x1B\\[[0-9;]*[mK]"); // matches ANSI color codes
        std::string processed_line = std::regex_replace(line, ansi_escape, ""); // remove ANSI codes

        // encode content
        std::string value = "[" + std::to_string(timestamp) + "," + processed_line + "]";
        db.set(count, value);

        if (count % 100 == 0)
            db.commit();

        if (echo)
            std::cout << line << std::endl;
    }

    // get the current timestamp in milliseconds since epoch
    long long get_current_timestamp()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
            .count();
    }

    void run()
    {
        std::string line;
        while (true)
        {
            if (!std::getline(std::cin, line)) // Check if input stream fails (EOF)
            {
                exit();
                break;
            }
            process_input(line);
        }
    }

    void exit()
    {
        std::cout << "Existing..." << std::endl;
        db.close();
    }

  private:
    DB db;
    std::atomic<key_type> line_count;
    bool echo;
};

static std::unique_ptr<processor> processor_ptr;

void signal_handler(int signum)
{
    if (processor_ptr)
        processor_ptr->exit();
    exit(signum);
}

int main()
{
    std::signal(SIGINT, signal_handler);

    std::string file = "./log.sqlite";
    std::string table = "log";
    bool echo = true;

    std::cout << app_title << " - Store lines from stdin into database: '" << file << "' table: '"
              << table << "'" << std::endl;

    try
    {
        processor_ptr = std::make_unique<processor>(file, table, echo);
        processor_ptr->run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
