#include "Sqlite.h"

using namespace TUI::Database;

/** Sqlite::Value */

Sqlite::Value::Value(std::nullptr_t)
	: _type(Type::null), _value{.null = nullptr} {}

Sqlite::Value::Value(int64_t integer)
	: _type(Type::integer), _value{.integer = integer} {}

Sqlite::Value::Value(double real)
	: _type(Type::real), _value{.real = real} {}

Sqlite::Value::Value(const std::string &text)
	: _type(Type::text), _value{.text = text} {}

Sqlite::Value::Value(std::string &&text)
	: _type(Type::text), _value{.text = std::move(text)} {}

Sqlite::Value::Value(const std::vector<uint8_t> &blob)
	: _type(Type::blob), _value{.blob = blob} {}

Sqlite::Value::Value(std::vector<uint8_t> &&blob)
	: _type(Type::blob), _value{.blob = std::move(blob)} {}

Sqlite::Value::Value(const Value &other)
	: _type(other._type), _value{}
{
	switch (_type) {
		case Type::null:
			_value.null = nullptr;
			break;
		case Type::integer:
			_value.integer = other._value.integer;
			break;
		case Type::real:
			_value.real = other._value.real;
			break;
		case Type::text:
			new (&_value.text) std::string(other._value.text);
			break;
		case Type::blob:
			new (&_value.blob) std::vector<uint8_t>(other._value.blob);
			break;
	}
}

Sqlite::Value::Value(Value &&other) noexcept
	: _type(other._type), _value{}
{
	switch (_type) {
		case Type::null:
			_value.null = nullptr;
			break;
		case Type::integer:
			_value.integer = other._value.integer;
			break;
		case Type::real:
			_value.real = other._value.real;
			break;
		case Type::text:
			new (&_value.text) std::string(std::move(other._value.text));
			break;
		case Type::blob:
			new (&_value.blob) std::vector<uint8_t>(std::move(other._value.blob));
			break;
	}
	other._type = Type::null;
	other._value.null = nullptr;
}

Sqlite::Value &Sqlite::Value::operator=(const Value &other)
{
	if (this != &other) {
		this->~Value();
		new (this) Value(other);
	}
	return *this;
}

Sqlite::Value &Sqlite::Value::operator=(Value &&other) noexcept
{
	if (this != &other) {
		this->~Value();
		new (this) Value(std::move(other));
	}
	return *this;
}

Sqlite::Value::~Value()
{
	switch (_type) {
		case Type::text:
			_value.text.~basic_string();
			break;
		case Type::blob:
			_value.blob.~vector();
			break;
		default:
			break;
	}
}

Sqlite::Value::Type Sqlite::Value::GetType() const
{
	return _type;
}

Sqlite::Value::operator std::nullptr_t() const
{
	if (_type != Type::null) {
		throw std::bad_cast();
	}
	return _value.null;
}

Sqlite::Value::operator int64_t() const
{
	if (_type != Type::integer) {
		throw std::bad_cast();
	}
	return _value.integer;
}

Sqlite::Value::operator double() const
{
	if (_type != Type::real) {
		throw std::bad_cast();
	}
	return _value.real;
}

Sqlite::Value::operator const std::string &() const
{
	if (_type != Type::text) {
		throw std::bad_cast();
	}
	return _value.text;
}

Sqlite::Value::operator std::string &()
{
	if (_type != Type::text) {
		throw std::bad_cast();
	}
	return _value.text;
}

Sqlite::Value::operator std::string &&() &&
{
	if (_type != Type::text) {
		throw std::bad_cast();
	}
	return std::move(_value.text);
}

Sqlite::Value::operator const std::vector<uint8_t> &() const
{
	if (_type != Type::blob) {
		throw std::bad_cast();
	}
	return _value.blob;
}

Sqlite::Value::operator std::vector<uint8_t> &()
{
	if (_type != Type::blob) {
		throw std::bad_cast();
	}
	return _value.blob;
}

Sqlite::Value::operator std::vector<uint8_t> &&() &&
{
	if (_type != Type::blob) {
		throw std::bad_cast();
	}
	return std::move(_value.blob);
}

/** UniqueSqlite3 */

Sqlite::UniqueSqlite3::UniqueSqlite3(sqlite3* db)
	: _db(db)
{
}

Sqlite::UniqueSqlite3::~UniqueSqlite3()
{
	if (_db) {
		sqlite3_close(_db);
	}
}

Sqlite::UniqueSqlite3::UniqueSqlite3(UniqueSqlite3 &&other) noexcept
	: _db(other._db)
{
	other._db = nullptr;
}

Sqlite::UniqueSqlite3 &Sqlite::UniqueSqlite3::operator=(UniqueSqlite3 &&other) noexcept
{
	if (this != &other) {
		if (_db) {
			sqlite3_close(_db);
		}
		_db = other._db;
		other._db = nullptr;
	}
	return *this;
}

Sqlite::UniqueSqlite3::operator sqlite3*() const
{
	return _db;
}

/** UniqueStmtInternal */

Sqlite::UniqueStmt::UniqueStmtInternal::UniqueStmtInternal(sqlite3_stmt* stmt)
	: _stmt(stmt)
{
}

Sqlite::UniqueStmt::UniqueStmtInternal::~UniqueStmtInternal()
{
	if (_stmt) 
	{
		sqlite3_finalize(_stmt);
	}
}

Sqlite::UniqueStmt::UniqueStmtInternal::UniqueStmtInternal(UniqueStmtInternal &&other) noexcept
	: _stmt(other._stmt)
{
	other._stmt = nullptr;
}

Sqlite::UniqueStmt::UniqueStmtInternal &Sqlite::UniqueStmt::UniqueStmtInternal::operator=(UniqueStmtInternal &&other) noexcept
{
	if (this != &other) {
		if (_stmt) {
			sqlite3_finalize(_stmt);
		}
		_stmt = other._stmt;
		other._stmt = nullptr;
	}
	return *this;
}

Sqlite::UniqueStmt::UniqueStmtInternal::operator sqlite3_stmt*() const
{
	return _stmt;
}

/** UniqueStmt */

Sqlite::UniqueStmt::operator sqlite3_stmt*() const
{
	return _stmt;
}

void Sqlite::UniqueStmt::BindValue(int i, std::string &&value)
{
	_bondStrings.push_back(std::move(value));
	auto& ref = _bondStrings.back();
	int rc = sqlite3_bind_text(_stmt, i, ref.c_str(), -1, SQLITE_STATIC);
	if (rc != SQLITE_OK)
	{
		_bondStrings.pop_back();
		throw std::runtime_error("Failed to bind text value: " + SqliteErrorToMessage(rc));
	}
}

void Sqlite::UniqueStmt::BindValue(int i, const std::string &value)
{
	BindValue(i, std::string(value));
}

void Sqlite::UniqueStmt::BindValue(int i, std::vector<uint8_t> &&value)
{
	_bondBlobs.push_back(std::move(value));
	auto& ref = _bondBlobs.back();
	int rc = sqlite3_bind_blob(_stmt, i, ref.data(), static_cast<int>(ref.size()), SQLITE_STATIC);
	if (rc != SQLITE_OK)
	{
		_bondBlobs.pop_back();
		throw std::runtime_error("Failed to bind blob value: " + SqliteErrorToMessage(rc));
	}
}

void Sqlite::UniqueStmt::BindValue(int i, const std::vector<uint8_t> &value)
{
	BindValue(i, std::vector<uint8_t>(value));
}

void Sqlite::UniqueStmt::BindValue(int i, int64_t value)
{
	int rc = sqlite3_bind_int64(_stmt, i, value);
	if (rc != SQLITE_OK)
	{
		throw std::runtime_error("Failed to bind integer value: " + SqliteErrorToMessage(rc));
	}
}

void Sqlite::UniqueStmt::BindValue(int i, double value)
{
	int rc = sqlite3_bind_double(_stmt, i, value);
	if (rc != SQLITE_OK)
	{
		throw std::runtime_error("Failed to bind real value: " + SqliteErrorToMessage(rc));
	}
}

void Sqlite::UniqueStmt::BindValue(int i, std::nullptr_t)
{
	int rc = sqlite3_bind_null(_stmt, i);
	if (rc != SQLITE_OK)
	{
		throw std::runtime_error("Failed to bind null value: " + SqliteErrorToMessage(rc));
	}
}

/** Sqlite */


Sqlite::Sqlite(Tev &tev)
	: _tev(tev), _workerThread(tev)
{
}

JS::Promise<std::shared_ptr<Sqlite>> Sqlite::CreateAsync(Tev& tev, const std::filesystem::path& dbPath)
{
	auto sqlite = std::shared_ptr<Sqlite>(new Sqlite(tev));
	int rc = 0;
	sqlite3* db = nullptr;
	rc = sqlite3_open(dbPath.string().c_str(), &db);
	sqlite->_db = UniqueSqlite3(db);
	if (rc != SQLITE_OK)
	{
		throw std::runtime_error("Failed to open main connection: " + SqliteErrorToMessage(rc));
	}
	/** Set the database to WAL mode */
	sqlite->Exec("PRAGMA journal_mode=WAL;");
	/** @todo other settings? */
	co_await sqlite->_workerThread.ExecTaskAsync([sqlite, dbPath](){
		/** Open the async connection in the worker thread */
		int rc = 0;
		sqlite3* db = nullptr;
		rc = sqlite3_open(dbPath.string().c_str(), &db);
		sqlite->_dbAsync = UniqueSqlite3(db);
		if (rc != SQLITE_OK)
		{
			throw std::runtime_error("Failed to open async connection: " + SqliteErrorToMessage(rc));
		}
	});
	co_return sqlite;
}

Sqlite::ExecResult Sqlite::ExecInternal(UniqueStmt&& stmt)
{
	ExecResult result{};
	bool finished = false;
	while(!finished)
	{
		int rc = sqlite3_step(stmt);
		switch (rc)
		{
		case SQLITE_DONE:
			finished = true;
			break;
		case SQLITE_BUSY:
			throw std::runtime_error("Database busy");
		    break;
		case SQLITE_MISUSE:
			throw std::runtime_error("Database misuse");
			break;
		case SQLITE_ROW:{
			int columnCount = sqlite3_column_count(stmt);
			std::map<std::string, Value> row;
			for (int i = 0; i < columnCount; i++)
			{
				std::string columnName{sqlite3_column_name(stmt, i)};
				int columnType = sqlite3_column_type(stmt, i);
				std::optional<Value> value{std::nullopt};
				switch(columnType)
				{
				case SQLITE_NULL:
					value = nullptr;
					break;
				case SQLITE_INTEGER:
					value = static_cast<int64_t>(sqlite3_column_int64(stmt, i));
					break;
				case SQLITE_FLOAT:
					value = sqlite3_column_double(stmt, i);
					break;
				case SQLITE_TEXT:{
					const unsigned char* text = sqlite3_column_text(stmt, i);
					if (!text)
					{
						value = nullptr;
						break;
					}
					/** This does not include the \0 */
					int textSize = sqlite3_column_bytes(stmt, i);
					value = std::string(reinterpret_cast<const char*>(text), textSize);			
				} break;
				case SQLITE_BLOB:{
					const void* blob = sqlite3_column_blob(stmt, i);
					if (!blob)
					{
						value = nullptr;
						break;
					}
					int blobSize = sqlite3_column_bytes(stmt, i);
					value = std::vector<uint8_t>(static_cast<const uint8_t*>(blob), static_cast<const uint8_t*>(blob) + blobSize);
				} break;
				default:
					break;
				}
				if (!value.has_value())
				{
					row.emplace(columnName, std::move(Value(nullptr)));
				}
				else
				{
					row.emplace(columnName, std::move(value.value()));
				}
			}
			result.push_back(std::move(row));
		} break;
		default:
			throw std::runtime_error("Database error: " + SqliteErrorToMessage(rc));
			break;
		}
	}
	return result;
}

std::string Sqlite::SqliteErrorToMessage(int rc)
{
	const char* rawMessage = sqlite3_errstr(rc);
	if (rawMessage)
	{
		return std::string(rawMessage);
	}
	else
	{
		return "Unknown error";
	}
}
