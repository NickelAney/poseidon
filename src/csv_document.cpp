// 这个文件是 Poseidon 服务器应用程序框架的一部分。
// Copyleft 2014 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "csv_document.hpp"
#include "string.hpp"
#include "profiler.hpp"
#include "log.hpp"
#include "buffer_streams.hpp"
#include "exception.hpp"

namespace Poseidon {

namespace {
	std::string escape_csv_field(const char *str, std::size_t len){
		PROFILE_ME;

		std::string field;
		field.reserve(len + 4);
		bool needs_quoting = false;
		for(std::size_t i = 0; i < len; ++i){
			const char ch = str[i];
			if(ch == '\"'){
				field += '\"';
				needs_quoting = true;
			} else if((ch == ',') || (ch == '\r') || (ch == '\n')){
				needs_quoting = true;
			}
			field += ch;
		}
		if(needs_quoting){
			field.insert(field.begin(), '\"');
			field.insert(field.end(), '\"');
		}
		return field;
	}
}

CsvDocument::CsvDocument(std::istream &is)
	: m_elements()
{
	parse(is);
	DEBUG_THROW_UNLESS(is, Exception, sslit("CsvDocument parser error"));
}

void CsvDocument::reset_header(const boost::container::map<SharedNts, std::string> &row){
	VALUE_TYPE(m_elements) elements;
	for(AUTO(it, row.begin()); it != row.end(); ++it){
		elements[it->first];
	}
	m_elements.swap(elements);
}
#ifdef POSEIDON_CXX11
void CsvDocument::reset_header(std::initializer_list<SharedNts> header){
	VALUE_TYPE(m_elements) elements;
	for(AUTO(it, header.begin()); it != header.end(); ++it){
		elements[*it];
	}
	m_elements.swap(elements);
}
#endif
void CsvDocument::append(const boost::container::map<SharedNts, std::string> &row){
	AUTO(it, m_elements.begin());
	try {
		while(it != m_elements.end()){
			std::string value;
			const AUTO(rit, row.find(it->first));
			if(rit != row.end()){
				value = rit->second;
			}
			it->second.push_back(STD_MOVE(value));
			++it;
		}
	} catch(...){
		while(it != m_elements.begin()){
			--it;
			it->second.pop_back();
		}
		throw;
	}
}
#ifdef POSEIDON_CXX11
void CsvDocument::append(boost::container::map<SharedNts, std::string> &&row){
	AUTO(it, m_elements.begin());
	try {
		while(it != m_elements.end()){
			std::string value;
			const AUTO(rit, row.find(it->first));
			if(rit != row.end()){
				value = std::move(rit->second);
			}
			it->second.push_back(STD_MOVE(value));
			++it;
		}
	} catch(...){
		while(it != m_elements.begin()){
			--it;
			it->second.pop_back();
		}
		throw;
	}
}
#endif

StreamBuffer CsvDocument::dump() const {
	PROFILE_ME;

	Buffer_ostream bos;
	dump(bos);
	return STD_MOVE(bos.get_buffer());
}
void CsvDocument::dump(std::ostream &os) const {
	PROFILE_ME;

	AUTO(it, m_elements.begin());
	if(it == m_elements.end()){
		return;
	}

	os <<escape_csv_field(it->first, std::strlen(it->first));
	while(++it != m_elements.end()){
		os <<',' <<escape_csv_field(it->first, std::strlen(it->first));
	}
	os <<std::endl;

	std::size_t row = 0;
	for(;;){
		it = m_elements.begin();
		if(row >= it->second.size()){
			break;
		}
		os <<escape_csv_field(it->second.at(row).data(), it->second.at(row).size());
		while(++it != m_elements.end()){
			os <<',' <<escape_csv_field(it->second.at(row).data(), it->second.at(row).size());
		}
		os <<std::endl;
		++row;
	}
}
void CsvDocument::parse(std::istream &is){
	PROFILE_ME;

	VALUE_TYPE(m_elements) elements;

	boost::container::vector<boost::container::vector<std::string> > matrix;
	boost::container::vector<std::string> line;
	std::size_t count = 0;
	typedef std::istream::traits_type traits;
	traits::int_type next = is.peek();
	for(;;){
		line.clear();
		std::string seg;
		enum {
			quote_init,
			quote_open,
			quote_closing,
			quote_closed,
		} quote_state = quote_init;
		for(; !traits::eq_int_type(next, traits::eof()); next = is.peek()){
			const char ch = traits::to_char_type(is.get());
			if(quote_state == quote_init){
				if(ch == '\"'){
					quote_state = quote_open;
					continue;
				}
				quote_state = quote_closed;
			} else if(quote_state == quote_open){
				if(ch == '\"'){
					quote_state = quote_closing;
					continue;
				}
				seg += ch;
				// quote_state = quote_open;
				continue;
			} else if(quote_state == quote_closing){
				if(ch == '\"'){
					seg += '\"';
					quote_state = quote_open;
					continue;
				}
				quote_state = quote_closed;
			}
			if(ch == '\n'){
				if(!seg.empty() && (*seg.rbegin() == '\r')){
					seg.erase(seg.end() - 1);
				}
				line.push_back(trim(STD_MOVE(seg)));
				next = is.peek();
				break;
			} else if(ch == ','){
				line.push_back(trim(STD_MOVE(seg)));
				seg.clear();
				quote_state = quote_init;
			} else {
				seg += ch;
			}
		}
		if(line.empty() || ((line.size() == 1) && line.front().empty())){
			if(traits::eq_int_type(next, traits::eof())){
				break;
			}
			LOG_POSEIDON_WARNING("Ignoring empty line ", count);
			continue;
		}
		++count;

		if(matrix.empty()){
			if(line.empty()){
				is.setstate(std::ios::failbit);
				return;
			}
			matrix.resize(line.size());
			for(std::size_t i = 0; i < line.size(); ++i){
				for(std::size_t j = 0; j < i; ++j){
					if(matrix.at(j).at(0) == line.at(i)){
						LOG_POSEIDON_WARNING("Duplicate CSV header on line ", count, ": ", line.at(i));
						is.setstate(std::ios::failbit);
						return;
					}
				}
				matrix.at(i).push_back(STD_MOVE(line.at(i)));
			}
		} else {
			if(line.size() != matrix.size()){
				LOG_POSEIDON_WARNING("Inconsistent CSV column count on line ", count, ": got ", line.size(), ", expecting ", matrix.size());
				is.setstate(std::ios::failbit);
				return;
			}
			for(std::size_t i = 0; i < line.size(); ++i){
				matrix.at(i).push_back(STD_MOVE(line.at(i)));
			}
		}
	}
	for(AUTO(it, matrix.begin()); it != matrix.end(); ++it){
		SharedNts key(it->at(0));
		it->erase(it->begin());
		elements.emplace(STD_MOVE_IDN(key), STD_MOVE_IDN(*it));
	}

	m_elements.swap(elements);
}

}
