#include <gtest/gtest.h>

#include <map>
#include <sstream>
#include <string>

#include <boost/filesystem.hpp>
#include <sqlite3.h>

#include "prioritydb.h"

#define DEFAULT_MAX_SIZE 100000000LL


namespace fs = boost::filesystem;

class DBFixture : public ::testing::Test {
  protected:
    typedef std::map<std::string, std::string> Record;

    virtual void SetUp() {
        db_path_ = fs::temp_directory_path() / fs::path{"prism_test.db"};
        db_string_ = db_path_.native();
        table_name_ = "prism_data";
        fs::remove(db_path_);
    }

    virtual void TearDown() {
        fs::remove(db_path_);
    }

    sqlite3* open_db_() {
        sqlite3* sqlite_db;
        if (sqlite3_open(db_string_.data(), &sqlite_db) != SQLITE_OK) {
            throw PriorityDBException{sqlite3_errmsg(sqlite_db)};
        }
        return sqlite_db;
    }

    int close_db_(sqlite3* db) {
        return sqlite3_close(db);
    }

    static int callback_(void* response_ptr, int num_values, char** values, char** names) {
        auto response = (std::vector<Record>*) response_ptr;
        auto record = Record();
        for (int i = 0; i < num_values; ++i) {
            if (values[i]) {
                record[names[i]] = values[i];
            }
        }

        response->push_back(record);

        return 0;
    }

    std::vector<Record> execute_(const std::string& sql) {
        std::vector<Record> response;
        auto db = open_db_();
        char* error;
        int rc = sqlite3_exec(db, sql.data(), &callback_, &response, &error);
        if (rc != SQLITE_OK) {
            auto error_string = std::string{error};
            sqlite3_free(error);
            throw PriorityDBException{error_string};
        }

        return response;
    }

    fs::path db_path_;
    std::string db_string_;
    std::string table_name_;
};

TEST_F(DBFixture, EmptyDBTest) {
    EXPECT_FALSE(fs::exists(db_path_));
}

TEST_F(DBFixture, ConstructDBTest) {
    ASSERT_FALSE(fs::exists(db_path_));
    PriorityDB db{DEFAULT_MAX_SIZE, db_string_};
    EXPECT_TRUE(fs::exists(db_path_));
}

TEST_F(DBFixture, ConstructDBNoDestructTest) {
    ASSERT_FALSE(fs::exists(db_path_));
    {
        PriorityDB db{DEFAULT_MAX_SIZE, db_string_};
        EXPECT_TRUE(fs::exists(db_path_));
    }
    EXPECT_TRUE(fs::exists(db_path_));
}

TEST_F(DBFixture, ConstructDBMultipleTest) {
    ASSERT_FALSE(fs::exists(db_path_));
    {
        PriorityDB db{DEFAULT_MAX_SIZE, db_string_};
        ASSERT_TRUE(fs::exists(db_path_));
    }
    {
        PriorityDB db{DEFAULT_MAX_SIZE, db_string_};
        EXPECT_TRUE(fs::exists(db_path_));
    }
    EXPECT_TRUE(fs::exists(db_path_));
}

TEST_F(DBFixture, ConstructThrowTest) {
    bool thrown = false;
    try {
        PriorityDB db{DEFAULT_MAX_SIZE, (fs::temp_directory_path() / fs::path{""}).native()};
    } catch (const PriorityDBException& e) {
        thrown = true;
        EXPECT_EQ(std::string{"unable to open database file"},
                  std::string{e.what()});
    }
    EXPECT_TRUE(thrown);
}

TEST_F(DBFixture, ConstructCurrentThrowTest) {
    bool thrown = false;
    try {
        PriorityDB db{DEFAULT_MAX_SIZE, (fs::temp_directory_path() / fs::path{"."}).native()};
    } catch (const PriorityDBException& e) {
        thrown = true;
        EXPECT_EQ(std::string{"unable to open database file"},
                  std::string{e.what()});
    }
    EXPECT_TRUE(thrown);
}

TEST_F(DBFixture, ConstructParentThrowTest) {
    bool thrown = false;
    try {
        PriorityDB db{DEFAULT_MAX_SIZE, (fs::temp_directory_path() / fs::path{".."}).native()};
    } catch (const PriorityDBException& e) {
        thrown = true;
        EXPECT_EQ(std::string{"unable to open database file"},
                  std::string{e.what()});
    }
    EXPECT_TRUE(thrown);
}

TEST_F(DBFixture, ConstructZeroSpaceTest) {
    bool thrown = false;
    try {
        PriorityDB db{0LL, db_string_};
    } catch (const PriorityDBException& e) {
        thrown = true;
        EXPECT_EQ(std::string{"Must specify a nonzero max_size"},
                  std::string{e.what()});
    }
    EXPECT_TRUE(thrown);
}

TEST_F(DBFixture, InitialDBTest) {
    ASSERT_FALSE(fs::exists(db_path_));
    PriorityDB db{DEFAULT_MAX_SIZE, db_string_};
    ASSERT_TRUE(fs::exists(db_path_));
    std::stringstream stream;
    stream << "SELECT name FROM sqlite_master WHERE type='table' AND name='"
           << table_name_
           << "';";
    auto response = execute_(stream.str());
    ASSERT_EQ(1, response.size());
    auto record = response[0];
    ASSERT_EQ(1, record.size());
    ASSERT_NE(record.end(), record.find("name"));
    EXPECT_EQ(std::string{"prism_data"}, record["name"]);
}

TEST_F(DBFixture, InitialEmptyDBTest) {
    ASSERT_FALSE(fs::exists(db_path_));
    PriorityDB db{DEFAULT_MAX_SIZE, db_string_};
    ASSERT_TRUE(fs::exists(db_path_));
    std::stringstream stream;
    stream << "SELECT * FROM "
           << table_name_
           << ";";
    auto response = execute_(stream.str());
    EXPECT_EQ(0, response.size());
}

TEST_F(DBFixture, InitialDBAfterDestructorTest) {
    ASSERT_FALSE(fs::exists(db_path_));
    {
        PriorityDB db{DEFAULT_MAX_SIZE, db_string_};
        ASSERT_TRUE(fs::exists(db_path_));
    }
    ASSERT_TRUE(fs::exists(db_path_));
    std::stringstream stream;
    stream << "SELECT name FROM sqlite_master WHERE type='table' AND name='"
           << table_name_
           << "';";
    auto response = execute_(stream.str());
    ASSERT_EQ(1, response.size());
    auto record = response[0];
    ASSERT_EQ(1, record.size());
    ASSERT_NE(record.end(), record.find("name"));
    EXPECT_EQ(std::string{"prism_data"}, record["name"]);
}

TEST_F(DBFixture, InitialEmptyDBAfterDestructorTest) {
    ASSERT_FALSE(fs::exists(db_path_));
    {
        PriorityDB db{DEFAULT_MAX_SIZE, db_string_};
        ASSERT_TRUE(fs::exists(db_path_));
    }
    ASSERT_TRUE(fs::exists(db_path_));
    std::stringstream stream;
    stream << "SELECT * FROM "
           << table_name_
           << ";";
    auto response = execute_(stream.str());
    EXPECT_EQ(0, response.size());
}

TEST_F(DBFixture, InsertEmptyHashTest) {
    PriorityDB db{DEFAULT_MAX_SIZE, db_string_};
    db.Insert(1, "", 5, false);
    std::stringstream stream;
    stream << "SELECT * FROM "
           << table_name_
           << ";";
    auto response = execute_(stream.str());
    ASSERT_EQ(0, response.size());
}

TEST_F(DBFixture, InsertSingleTest) {
    PriorityDB db{DEFAULT_MAX_SIZE, db_string_};
    db.Insert(1, "hash", 5, false);
    std::stringstream stream;
    stream << "SELECT * FROM "
           << table_name_
           << ";";
    auto response = execute_(stream.str());
    ASSERT_EQ(1, response.size());
    auto record = response[0];
    ASSERT_EQ(5, record.size());
    EXPECT_EQ(1, std::stoi(record["id"]));
    EXPECT_EQ(1, std::stoi(record["priority"]));
    EXPECT_EQ(std::string{"hash"}, record["hash"]);
    EXPECT_EQ(false, std::stoi(record["on_disk"]));
}

TEST_F(DBFixture, InsertCoupleTest) {
    PriorityDB db{DEFAULT_MAX_SIZE, db_string_};
    db.Insert(1, "hash", 5, false);
    db.Insert(3, "hashbrowns", 10, true);
    std::stringstream stream;
    stream << "SELECT * FROM "
           << table_name_
           << ";";
    auto response = execute_(stream.str());
    ASSERT_EQ(2, response.size());
    {
        auto record = response[0];
        ASSERT_EQ(5, record.size());
        EXPECT_EQ(1, std::stoi(record["id"]));
        EXPECT_EQ(1, std::stoi(record["priority"]));
        EXPECT_EQ(std::string{"hash"}, record["hash"]);
        EXPECT_EQ(5, std::stoi(record["size"]));
        EXPECT_EQ(false, std::stoi(record["on_disk"]));
    }
    {
        auto record = response[1];
        ASSERT_EQ(5, record.size());
        EXPECT_EQ(2, std::stoi(record["id"]));
        EXPECT_EQ(3, std::stoi(record["priority"]));
        EXPECT_EQ(std::string{"hashbrowns"}, record["hash"]);
        EXPECT_EQ(10, std::stoi(record["size"]));
        EXPECT_EQ(true, std::stoi(record["on_disk"]));
    }
}

TEST_F(DBFixture, InsertManyTest) {
    PriorityDB db{DEFAULT_MAX_SIZE, db_string_};
    auto number_of_records = 100;
    for (int i = 0; i < number_of_records; ++i) {
        db.Insert(i, std::to_string(i * i), i * 2, i % 2);
    }
    std::stringstream stream;
    stream << "SELECT * FROM "
           << table_name_
           << ";";
    auto response = execute_(stream.str());
    ASSERT_EQ(number_of_records, response.size());
    for (int i = 0; i < number_of_records; ++i) {
        auto record = response[i];
        ASSERT_EQ(5, record.size());
        EXPECT_EQ(i + 1, std::stoi(record["id"]));
        EXPECT_EQ(i, std::stoi(record["priority"]));
        EXPECT_EQ(std::to_string(i * i), record["hash"]);
        EXPECT_EQ(i * 2, std::stoi(record["size"]));
        EXPECT_EQ(i % 2, std::stoi(record["on_disk"]));
    }
}