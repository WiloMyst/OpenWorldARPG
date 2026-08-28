// 集成测试数据清理工具: 替代 mysql CLI (WSL 无 root 装不了 mysql-client)。
// 用与 game_server 相同的 libmysqlclient 连接参数删除测试账号数据。
// 用法: cleanup_test_data [host] [port] [user] [password] [database] [account...]
// 默认: 172.28.80.1 3306 game game_pass_2026 game_server m2inv aoiA aoiB
#include <mysql/mysql.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    const char* host = argc > 1 ? argv[1] : "172.28.80.1";
    const unsigned int port = argc > 2 ? static_cast<unsigned int>(std::atoi(argv[2])) : 3306;
    const char* user = argc > 3 ? argv[3] : "game";
    const char* password = argc > 4 ? argv[4] : "game_pass_2026";
    const char* database = argc > 5 ? argv[5] : "game_server";

    std::vector<std::string> accounts;
    for (int i = 6; i < argc; ++i) accounts.emplace_back(argv[i]);
    if (accounts.empty()) accounts = {"m2inv", "aoiA", "aoiB"};

    MYSQL* conn = mysql_init(nullptr);
    if (!conn) {
        std::fprintf(stderr, "[cleanup] mysql_init failed\n");
        return 1;
    }
    if (!mysql_real_connect(conn, host, user, password, database, port, nullptr, 0)) {
        std::fprintf(stderr, "[cleanup] connect failed: %s\n", mysql_error(conn));
        mysql_close(conn);
        return 1;
    }

    std::string in_list;
    for (size_t i = 0; i < accounts.size(); ++i) {
        if (i) in_list += ",";
        char escaped[256];
        mysql_real_escape_string(conn, escaped, accounts[i].c_str(),
                                 static_cast<unsigned long>(accounts[i].size()));
        in_list += "'" + std::string(escaped) + "'";
    }

    const std::string sql_inv = "DELETE FROM inventory_items WHERE account IN (" + in_list + ")";
    const std::string sql_players = "DELETE FROM players WHERE account IN (" + in_list + ")";
    if (mysql_query(conn, sql_inv.c_str()) != 0) {
        std::fprintf(stderr, "[cleanup] delete inventory_items failed: %s\n", mysql_error(conn));
        mysql_close(conn);
        return 1;
    }
    if (mysql_query(conn, sql_players.c_str()) != 0) {
        std::fprintf(stderr, "[cleanup] delete players failed: %s\n", mysql_error(conn));
        mysql_close(conn);
        return 1;
    }

    std::printf("[cleanup] test data removed for accounts: %s\n", in_list.c_str());
    mysql_close(conn);
    return 0;
}
