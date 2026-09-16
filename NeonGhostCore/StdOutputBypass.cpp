
#include "pch.h"

#include <assert.h>
#include <string>
#include <map>
#ifdef _WIN32
#include <Windows.h>
#endif
#include "StdOutputBypass.h"



using namespace ng;
using namespace io;

StdOutputBypass *StdOutputBypass::m_ptr = nullptr;
static StdOutputBypass test;



StdOutputBypass::StdOutputBypass() :old_cerr_buffer(nullptr), old_clog_buffer(nullptr), old_cout_buffer(nullptr)
{
	if (m_ptr == nullptr) {
		m_ptr = this;
		old_cout_buffer = std::cout.rdbuf();
		std::cout.rdbuf(GetCoutBuffer());

		old_cerr_buffer = std::cerr.rdbuf();
		std::cerr.rdbuf(GetCerrBuffer());

		old_clog_buffer = std::clog.rdbuf();
		std::clog.rdbuf(GetClogBuffer());

	}
	AddBypass(this);

}
void StdOutputBypass::AddBypass(StdOutputBypass *bypass)
{

	for (TStdOutIta i = m_ptr->m_bypass.begin(); i != m_ptr->m_bypass.end(); i++)
	{
		assert((*i != bypass));
	}

	m_ptr->m_bypass.push_back(bypass);
}
void StdOutputBypass::RemoveBypass(StdOutputBypass *bypass)
{
	for (TStdOutIta i = m_ptr->m_bypass.begin(); i != m_ptr->m_bypass.end(); i++)
	{
		if (*i == bypass)
		{
			m_ptr->m_bypass.erase(i);
			break;
		}
	}

}





StdOutputBypass::~StdOutputBypass()
{

	if (m_ptr == this)
	{
		if (old_cout_buffer != nullptr) std::cout.rdbuf(old_cout_buffer);
		if (old_cerr_buffer != nullptr) std::cerr.rdbuf(old_cerr_buffer);
		if (old_clog_buffer != nullptr) std::clog.rdbuf(old_clog_buffer);
	}

	RemoveBypass(this);

}


int StdOutputBypass::onCoutOverflowPrivate(char c)
{

	if (m_ptr != this) return 0;

	for (unsigned int i = 0; i < m_ptr->m_bypass.size(); i++)
	{
		m_ptr->m_bypass[i]->onCoutOverflow(c);
	}
	return 0;
}

int StdOutputBypass::onCerrOverflowPrivate(char c)
{

	if (m_ptr != this) return 0;

	for (unsigned int i = 0; i < m_ptr->m_bypass.size(); i++)
	{
		m_ptr->m_bypass[i]->onCerrOverflow(c);
	}
	return 0;
}

int StdOutputBypass::onClogOverflowPrivate(char c)
{

	if (m_ptr != this) return 0;

	for (unsigned int i = 0; i < m_ptr->m_bypass.size(); i++)
	{
		m_ptr->m_bypass[i]->onClogOverflow(c);
	}
	return 0;
}



 int StdOutputBypass::onCoutOverflow(char c) { 
	 if (old_cout_buffer != nullptr) old_cout_buffer->sputc(c);
	 return 0;

 }
 
 int StdOutputBypass::onClogOverflow(char c) 
 {
	 if (old_clog_buffer != nullptr) 	 old_clog_buffer->sputc(c);
	 return 0;

 }

 int StdOutputBypass::onCerrOverflow(char c) { 

	 if (old_cerr_buffer != nullptr) old_cerr_buffer->sputc(c);
	 return 0;
 }
 
