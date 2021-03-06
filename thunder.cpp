#include "thunder.h"

namespace thunder {

	/*!
	\brief 哨兵对象 sentry。
	\since build 1.01
	*/
	template<typename T>
	class sentry {
		T functor;
	public:
		sentry(T fun) :functor(std::move(fun)) {
		}

		sentry(sentry &&) = delete;
		sentry(sentry const &) = delete;

		~sentry() noexcept {
			static_assert(noexcept(functor()),
				"Please check that the finally block cannot throw, "
				"and mark the lambda as noexcept.");
			functor();
		}

	};

	template<typename T>
	sentry<T> finally(T o) {
		return{ std::move(o) };
	}

}

#define CAT(x,y) x##y
#define FINALLY_LINENAME(name,line) CAT(name,line)
#define FINALLY(callback) auto & FINALLY_LINENAME(EXIT,__LINE__) = thunder::finally(callback)


#include <string>



namespace thunder {

	std::string find_thunder_dir();

	//fuck sqlite call_back'data are UTF-8 endcoding blob_data
	std::string blob_tostring(const std::string& string);

	//save it,auto convert to utf-8
	std::string string_toblob(const std::string& string);
}


#include <sqlite\sqlite3.h>
#include <list>
#include "simpleregex.h"

int query_table_list(void* vec, int n_col, char** col_val, char** col_name);
int query_res_list(void* vec, int n_col, char** col_val, char** col_name);

int crack_highspeedstream(const char* thunder_dir) {
	std::string dir;
	if (!thunder_dir)
		dir = thunder::find_thunder_dir();
	else
		dir = std::string(thunder_dir);

	dir += "/Profiles/TaskDb.dat";

	sqlite3* data_base;
	auto open_result = sqlite3_open(dir.c_str(), &data_base);
	if (open_result != SQLITE_OK)
		throw std::runtime_error("数据库打开失败");
	FINALLY(
		[&]() noexcept {sqlite3_close(data_base);}
	);

	auto table_regexp = "superspeed_1_1$";

	std::list<std::string> tablenamelist;
	auto query_result = sqlite3_exec(data_base, "select name from sqlite_master where type='table' order by name;", query_table_list, &tablenamelist, nullptr);
	if (query_result != SQLITE_OK)
		throw std::runtime_error("数据库查询表名失败");

	for (auto& table_name : tablenamelist) {
		auto pair = match(table_regexp, table_name.c_str());

		if (pair.second) {
		
			std::printf("将要修改的表名为: %s\n", table_name.c_str());
			std::string sql_head = "select LocalTaskId,UserData from ";
			sql_head += table_name;
			std::list<std::string> resnamelist;
			std::list<std::string> idlist;
			std::pair<std::list<std::string>&, std::list<std::string>&> pair{ resnamelist,idlist };
			query_result = sqlite3_exec(data_base, sql_head.c_str(), query_res_list, &pair, nullptr);
			if (query_result != SQLITE_OK)
				throw std::runtime_error("数据库查询被举报资源信息失败");

			auto size = resnamelist.size();
			auto name_iter = resnamelist.begin();
			auto id_iter = idlist.begin();
			std::printf("以下任务将被修改:\n");
			for (unsigned i = 0; i != size;++i) {
				auto & name = *name_iter;
				auto pos = name.find("\"Result\":509");
				pos = pos == std::string::npos ? name.find("\"Result\":500") : pos;
				if (pos != std::string::npos) {
					{
						std::string sql_query = "select UserData from ";
						sql_query += table_name;
						sql_query += " where LocalTaskId = '";
						sql_query += *id_iter;
						sql_query += "'";
						sqlite3_stmt* stmt = nullptr;
						auto prepare_result = sqlite3_prepare(data_base, sql_query.c_str(), -1, &stmt, nullptr);
						if (prepare_result != SQLITE_OK)
							throw std::runtime_error("破解高速失败-编译查询信息代码失败");

						auto step_result = sqlite3_step(stmt);
						if (step_result != SQLITE_ROW)
							throw std::runtime_error("破解高速失败-查询信息失败");

						const void* blob = sqlite3_column_blob(stmt, 0);

						/*{
							auto f = std::fopen("blob.txt", "w");
							fprintf(f, (char*)blob);
							std::fclose(f);
						}*/
						auto str = utf8_tostring((char*)blob);
						printf("\t LocalTaskId: %s,UserData: %s\n", id_iter->c_str(),str.c_str());
						//to end
						sqlite3_step(stmt);
					}


					name.replace(pos, 12, "\"Result\":0");

					pos = name.find("\"Message\":");

					pos += 11;
					auto message = R"(\u6587\u4EF6\u540D\u4E2D\u5305\u542B\u8FDD\u89C4\u5185\u5BB9\uFF0C\u65E0\u6CD5\u6DFB\u52A0\u5230\u9AD8\u901F\u901A\u9053)";
					name.replace(pos, 60, message);

					auto blob_bin = thunder::string_toblob(name);

					std::string sql_update = "update ";
					sql_update += table_name;
					sql_update += " set UserData=? where LocalTaskId = '";
					sql_update += *id_iter;

					sql_update += "'";

					sqlite3_stmt* stmt = nullptr;
					auto pre_result = sqlite3_prepare(data_base, sql_update.c_str(), -1, &stmt, 0);
					if (pre_result != SQLITE_OK)
						throw std::runtime_error("破解高速失败-执行代码预编译");

					auto bind_result = sqlite3_bind_blob(stmt, 1, blob_bin.c_str(), blob_bin.size(), nullptr);
					if (bind_result != SQLITE_OK)
						throw std::runtime_error("破解高速失败-绑定数据失败");

					auto step_result = sqlite3_step(stmt);

					if (step_result != SQLITE_DONE)
						throw std::runtime_error("破解高速失败-提交操作失败");

				}

				++name_iter;

				++id_iter;
			}
			dir += "-journal";
			std::remove(dir.c_str());
			return resnamelist.size();
		}
	}

	return 0;
}

int query_table_list(void* vec, int n_col, char** col_val, char** col_name) {
	auto& list = *static_cast<std::list<std::string>*>(vec);

	for (unsigned i = 0;i < n_col;++i) {
		list.emplace_back(col_val[i]);
	}

	return 0;
}

#include <stdio.h>

int query_res_list(void* p, int n_col, char** col_val, char** col_name) {
	auto& pair = *static_cast<std::pair<std::list<std::string>&, std::list<std::string>&>*>(p);


	auto userdata = thunder::blob_tostring(col_val[1]);

	if (userdata.find("\"Result\":509") != std::string::npos || userdata.find("\"Result\":500") != std::string::npos) {
		pair.first.emplace_back(std::move(userdata));
		pair.second.emplace_back(col_val[0]);
	}


	return 0;
}

#include <Windows.h>

namespace thunder {

	std::string find_thunder_dir() {
		//Search 注册表
		HKEY regedit_key = nullptr;
		auto regedit_result = ::RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Thunder NetWork\\Thunder", 0, KEY_WOW64_64KEY | KEY_READ, &regedit_key);
		FINALLY(
			[&]() noexcept {RegCloseKey(regedit_key);}
		);
		if (regedit_result != ERROR_SUCCESS)
			throw std::runtime_error("没有找到迅雷相关的注册表信息");


		DWORD cbData = 0;
		auto getvalue_result = ::RegGetValueA(
			regedit_key, nullptr, "Path",
			RRF_RT_REG_SZ, nullptr,
			nullptr, &cbData
			);
		if (getvalue_result != ERROR_SUCCESS)
			throw std::runtime_error("获取注册表键值长度失败");

		std::string dir(static_cast<size_t>(cbData), char());
		getvalue_result = ::RegGetValueA(
			regedit_key, nullptr, "Path",
			RRF_RT_REG_SZ, nullptr,
			&dir[0], &cbData
			);
		if (getvalue_result != ERROR_SUCCESS)
			throw std::runtime_error("获取注册表键值内容失败");

		auto pos = dir.find("Program\\Thunder.exe");

		if (pos == std::string::npos || pos <= 1)
			throw std::runtime_error("迅雷的路径存在错误");

		dir.resize(pos - 1);


		return dir;
	}

	//UTF-8，转成UTF-16
	//当做CP_UTF8
	std::string blob_tostring(const std::string& string) {
		const int w_Len(::MultiByteToWideChar(CP_UTF8, 0, string.c_str(), string.length(), {}, 0));
		std::wstring wstr(w_Len, wchar_t());
		wchar_t * w_str = &wstr[0];
		::MultiByteToWideChar(CP_UTF8, 0, string.c_str(), string.length(), w_str, w_Len);

		std::string str((char *)w_str);
		str.push_back('}');
		return str;
	}

	//CP_UTF8当做UTF-16
	//UTF-16 转UTF-8
	std::string string_toblob(const std::string& string) {
		/*const int m_Len(::WideCharToMultiByte(CP_UTF8, 0, (const wchar_t*)string.c_str(), string.length() / 2, {}, 0, {}, {}));
		std::string mstr(m_Len, wchar_t());
		char * m_str = &mstr[0];
		::WideCharToMultiByte(CP_UTF8, 0, (const wchar_t*)string.c_str(), string.length() / 2, m_str, m_Len, {}, {});
		return mstr;*/


		return string;
	}
}