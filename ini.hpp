#pragma once
#include <fstream>
#include <filesystem>
#include <sstream>
#include <map>
#include <set>
#include <string>



class IniFile {

public:

	struct Section {
		friend class IniFile;
		std::string &operator[](const std::string &key) noexcept;
		std::string &get(const std::string &key);
		void remove(const std::string &key);
	private:
		using keymap = std::map<std::string, std::string>;
		keymap m_keys;
		using addedkeys = std::set<std::string>;
		addedkeys m_addedKeys;
	};

private:

	std::filesystem::path m_path;
	using secmap = std::map<std::string, Section>;
	secmap m_sections;
	using addedsecs = std::set<std::string>;
	addedsecs m_addedSections;

public:

	Section &operator [](const std::string &section) noexcept;
	Section &get(const std::string &section);
	void remove(const std::string &section, bool removeKeys = false);

	struct Options {
		enum LINE_ENDINGS { CRLF, LF, CR };
		LINE_ENDINGS save_ln_endings = CRLF;

		enum COMMENT_SIGN { BOTH, HASH, SCLN };
		COMMENT_SIGN read_comment_sign = BOTH;

		enum STRINGS {};
	} options;

	inline IniFile() = default;
	inline IniFile(const std::filesystem::path &path);
	~IniFile() = default;

	void open(const std::filesystem::path &path);
	void reopen();

	void save(const std::filesystem::path &newPath);
	void save();
	void print() const;

	static void trim(std::string &s);

private:


	static std::vector<char> load_file(const std::filesystem::path &file);

	enum class LINE_T { NL, INVALID, SEC, KEYVAL, COMMENT };

	struct Line {
		LINE_T type = LINE_T::NL;
		std::string content, value;
		bool nlAfter = false;
	};
	Line parseLine(const std::vector<char> &src, size_t *offset) const;
	std::vector<Line> m_cachedLines;

};



#pragma region definitions

#ifdef INI_IMPLEMENTATION

std::vector<char> IniFile::load_file(const std::filesystem::path &p) {
	std::fstream f(p, std::ios::in | std::ios::binary);
	if(!f.good()) return {};
	auto fs = std::filesystem::file_size(p);
	std::vector<char> r(fs);
	f.read(r.data(), fs);
	return r;
}

void IniFile::trim(std::string &s) {
	if(s.empty()) return;
	size_t i_act = 0, trimLen = 0;
	for(size_t i = 0; i < s.length(); i++) {
		bool isSpace = s[i] == ' ';
		if(i_act || !isSpace) s[i_act++] = s[i];
		if(!isSpace) trimLen = i_act;
	}
	s.resize(trimLen);
}

IniFile::IniFile(const std::filesystem::path &path) {
	open(path);
}

IniFile::Section &IniFile::operator [](const std::string &section) noexcept {
	auto foundSection = m_sections.find(section);
	if(foundSection != m_sections.end()) return foundSection->second;
	m_addedSections.insert(section);
	return m_sections[section];
}
IniFile::Section &IniFile::get(const std::string &section) {
	return m_sections.at(section);
}
void IniFile::remove(const std::string &section, bool removeKeys) {
	auto foundSection = m_sections.find(section);
	if(foundSection == m_sections.end()) return;

	if(section == "") return;
	if(!removeKeys) {
		auto &root = m_sections[""];
		for(auto &key : foundSection->second.m_keys) {
			root.m_keys.insert(key);
			root.m_addedKeys.insert(key.first);
		}
	}
	m_sections.erase(foundSection);
	m_addedSections.erase(section);
}

std::string &IniFile::Section::operator[](const std::string &key) noexcept {
	auto foundKey = m_keys.find(key);
	if(foundKey != m_keys.end()) return foundKey->second;
	m_addedKeys.insert(key);
	return m_keys[key];
}
std::string &IniFile::Section::get(const std::string &key) {
	return m_keys.at(key);
}
void IniFile::Section::remove(const std::string &key) {
	auto foundKey = m_keys.find(key);
	if(foundKey == m_keys.end()) return;
	m_keys.erase(foundKey);
	m_addedKeys.erase(key);
}

IniFile::Line IniFile::parseLine(const std::vector<char> &src, size_t *offset) const {
	using namespace std;
	auto type = LINE_T::NL;
	std::string lineContent, key, val;

	for(size_t lnc = 0; *offset < src.size() && src[*offset] != '\n'; ++*offset, lnc++) {
		if(src[*offset] == '\r') continue;
		lineContent += src[*offset];
	}
	trim(lineContent);

	if(lineContent.empty()) {
		return { LINE_T::NL, string() };
	}

	if(lineContent[0] == ';' || lineContent[0] == '#') {
		return { LINE_T::COMMENT, move(lineContent) };
	}

	if(lineContent[0] == '[') {
		lineContent[0] = ' ';
		size_t closingBrack = lineContent.find(']');
		if(closingBrack != string::npos)
			lineContent.resize(closingBrack);
		trim(lineContent);
		return { LINE_T::SEC, move(lineContent) };
	}

	size_t eqpos = lineContent.find_first_of('=');
	if(eqpos != std::string::npos) {
		key = lineContent.substr(0, eqpos);
		trim(key);
		val = lineContent.substr(eqpos + 1);
		trim(val);
		return { LINE_T::KEYVAL, move(key), move(val) };
	}

	return { LINE_T::INVALID, move(lineContent) };
}

void IniFile::open(const std::filesystem::path &path) {
	m_sections.clear();
	m_cachedLines.clear();

	m_path = path;
	auto src = load_file(path);

	auto *selectedSection = &m_sections[""];

	for(size_t offset = 0; offset < src.size(); offset++) {

		Line line = parseLine(src, &offset);
		if(line.type == LINE_T::NL) {
			if(m_cachedLines.size()) {
				m_cachedLines.back().nlAfter = true;
			}
			continue;
		}
		m_cachedLines.push_back(line);

		switch(line.type) {
			case LINE_T::SEC:
				selectedSection = &m_sections[line.content];
				break;
			case LINE_T::KEYVAL:
				selectedSection->m_keys[line.content] = line.value;
				break;
		}
	}
}

void IniFile::reopen() {
	if(!m_path.empty())
		open(m_path);
}

void IniFile::save(const std::filesystem::path &newPath) {
	m_path = newPath;
	save();
}

void IniFile::save() {

	std::vector<Line> newLines;

	//std::map<secmap::iterator, std::set<Section::keymap::iterator>> saved;
	std::map<std::string, std::set<std::string>> saved;
	saved[""];
	auto currSec = m_sections.find("");
	bool isFirst = true;

	// build new lines

	/*
	iterate over cached lines
	if comment or invalid - move to new
	if sec - check if 
	add new sections - repeat
	*/
	for(auto &line : m_cachedLines) {

		if(line.type != LINE_T::SEC && currSec == m_sections.end()) continue;

		if(line.type == LINE_T::SEC) {

			if(currSec != m_sections.end() && isFirst) {
				// add all new (added) keys
				auto &savedKeys = saved[currSec->first];
				for(auto &key : currSec->second.m_addedKeys) {
					savedKeys.insert(key);
					newLines.push_back({LINE_T::KEYVAL, key, currSec->second.m_keys[key]});
				}
			}

			currSec = m_sections.find(line.content);
			if(currSec == m_sections.end()) continue;

			newLines.push_back(line);

			isFirst = saved.find(currSec->first) == saved.end();
			saved[currSec->first];

		}

		if(line.type == LINE_T::KEYVAL) {
			auto foundKey = currSec->second.m_keys.find(line.content);
			if(foundKey == currSec->second.m_keys.end()) continue;
			auto &savedKeys = saved[currSec->first];
			if(savedKeys.find(foundKey->first) == savedKeys.end()) {
				savedKeys.insert(foundKey->first);
				newLines.push_back({ LINE_T::KEYVAL, foundKey->first, foundKey->second, line.nlAfter });
			}
		}
		if(line.type == LINE_T::COMMENT || line.type == LINE_T::INVALID) {
			newLines.push_back(line);
		}
	}

	// add new sections
	for(auto &sec : m_addedSections) {
		currSec = m_sections.find(sec);
		if(currSec == m_sections.end()) throw 0;
		if(saved.find(sec) != saved.end()) continue;

		newLines.push_back({LINE_T::SEC, sec});
		for(auto &keyval : currSec->second.m_keys) {
			newLines.push_back({LINE_T::KEYVAL, keyval.first, keyval.second});
		}
	}

	// clear added
	m_addedSections.clear();
	for(auto &sec : m_sections) {
		sec.second.m_addedKeys.clear();
	}

	// build file content
	std::stringstream fileStream;
	if(newLines.size()) {
		newLines.back().nlAfter = false;
		fileStream << "\n";
	}
	bool firstLine = true;
	// *offset from last non-comment line*
	int offsetNC = 0;
	for(Line &line : newLines) {
		switch(line.type) {
			case LINE_T::SEC:
				if(2 - offsetNC > 0 && !firstLine) fileStream << (2 - offsetNC == 1 ? "\n" : "\n\n");
				fileStream << '[' << line.content << ']';
				break;
			case LINE_T::KEYVAL:
				fileStream << line.content << " = " << line.value;
				break;
			case LINE_T::COMMENT:
			case LINE_T::INVALID:
				fileStream << line.content;
				break;
		}
		if(line.type != LINE_T::COMMENT) {
			offsetNC = line.nlAfter ? 1 : 0;
		}
		fileStream << (line.nlAfter ? "\n\n" : "\n");

		if(firstLine) firstLine = false;
	}

	// flush
	std::fstream file(m_path, std::ios::out | std::ios::binary | std::ios::trunc);
	if(!file.good()) return;
	auto fsize = std::filesystem::file_size(m_path);
	if(fsize > fileStream.tellp())
		std::filesystem::resize_file(m_path, static_cast<size_t>(fileStream.tellp()));
	file << fileStream.rdbuf();
	std::string test = fileStream.str();
}


#ifdef INI_IMPLEMENTATION_PRINT
#include <iostream>
void IniFile::print() const {

	printf("\n- cached lines -\n");
	auto typestr = [](LINE_T t) {
		switch(t) {
			case LINE_T::SEC: return "sec";
			case LINE_T::KEYVAL: return "key";
			case LINE_T::COMMENT: return "com";
			case LINE_T::INVALID: return "inv";
			default: return "<>";
		}
		};
	for(auto &line : m_cachedLines) {
		printf("[%s] [%s] [%s]\n%s", typestr(line.type), line.content.c_str(), line.value.c_str(), line.nlAfter ? "\n" : "");
	}

	printf("\n- sections -\n");
	for(auto &sec : m_sections) {
		if(m_addedSections.find(sec.first) != m_addedSections.end()) continue;
		printf("[%s]\n", sec.first.c_str());
		for(auto &keyval : sec.second.m_keys) {
			if(sec.second.m_addedKeys.find(keyval.first) != sec.second.m_addedKeys.end()) continue;
			printf("%s = %s\n", keyval.first.c_str(), keyval.second.c_str());
		}
		if(!sec.second.m_addedKeys.empty()) printf("- added keys -\n");
		for(auto &key : sec.second.m_addedKeys) {
			printf("%s = %s\n", key.c_str(), sec.second.m_keys.at(key).c_str());
		}
		printf("\n");
	}
	if(m_addedSections.empty()) return;
	printf("- added sections -\n\n");
	for(auto &sec : m_addedSections) {
		printf("[%s]\n", sec.c_str());
		for(auto &keyval : m_sections.at(sec).m_keys) {
			printf("%s = %s\n", keyval.first.c_str(), keyval.second.c_str());
		}
	}
}
#endif

#endif

#pragma endregion