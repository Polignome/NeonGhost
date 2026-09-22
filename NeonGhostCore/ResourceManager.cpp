#include "pch.h"
#include <chrono>
#include <filesystem>
#include <thread>
#include <queue>
#include "ResourceManager.h"
///////////////////////////////////////////////////////////////////////////
//
//  Wichtig 29.03.2020
//  Dieser Thread läuft jetzt durch. 
//
//
//
///////////////////////////////////////////////////////////////////////////



using namespace ng;
using namespace resource;





////////////////////////////////////////////////////////////////////////////////////////////////////////////

BaseResourceManager::BaseResourceManager(std::string name) :m_name(name) {
}

void BaseResourceManager::RegisterMe() {
	Manager::AddResMngr(this);
}

void BaseResourceManager::RemoveMe()
{

	Manager::RemoveResMngr(this);
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                        //
//                                                                                                        //
//                                                                                                        //
//                                    Manager                                                             //
//                                                                                                        //
//                                                                                                        //
//                                                                                                        //
////////////////////////////////////////////////////////////////////////////////////////////////////////////

Manager* Manager::m_instance = nullptr;


std::string Manager::m_resource_path = std::filesystem::current_path().string();

std::queue<Manager::TResToLoadQurey> Manager::m_res_queue;
void* Manager::m_opengl_contexed = nullptr;
#ifdef _NG_USE_RES_THREADS	
std::thread* Manager::m_res_load_thread = nullptr;
std::atomic_bool Manager::m_res_query_ready_to_read_or_write(true);
std::mutex Manager::m_res_query_look;
#endif


std::string Manager::GetResourcePath() {
	return GetInstance()->_GetResourcePath();
}

void Manager::SetResourcePath(std::string path) {
	GetInstance()->_SetResourcePath(path);

}

std::string Manager::_GetResourcePath() {
	return m_resource_path;
}

void Manager::_SetResourcePath(std::string path) {
	m_resource_path = path;

}



void Manager::UnlockMutex() {
#ifdef _NG_USE_RES_THREADS	

	GetInstance()->m_res_query_look.unlock();
#endif
}


void Manager::LockMutex() {
#ifdef _NG_USE_RES_THREADS	


	GetInstance()->m_res_query_look.lock();
#endif
}

void Manager::SetOpenGlContexed(void* contexed) {
	GetInstance()->m_opengl_contexed = contexed;
}




bool Manager::IsResourceManager(std::string name) {
	return GetInstance()->_IsResourceManager(name);
}

void Manager::Flush() {

	GetInstance()->_Flush();
}

unsigned long Manager::LoadResourceFile(std::string filename) {

	TResToLoadQurey new_entry;
	new_entry.m_resource_manager = "";
	new_entry.m_resource_name = "";
	new_entry.m_filename = filename;
	new_entry.m_file_flags = 0;
	new_entry.m_resource_flags = 0;
	new_entry.m_xml_file = true;

#ifdef _NG_USE_RES_THREADS	
	m_res_query_look.lock();
#endif
	m_res_queue.push(new_entry);
#ifdef _NG_USE_RES_THREADS	
	m_res_query_look.unlock();

	if (m_res_load_thread != nullptr)
	{
		return 0;
	}




	m_res_load_thread = new std::thread(Manager::ResourceDownloadThread, m_opengl_contexed);
#else
	Manager::ResourceDownloadThread(m_opengl_contexed);
#endif


	return 0;
}

// Unsericher !!
std::vector<BaseResourceManager*> Manager::GetResourceManagerList() {
	return GetInstance()->_GetResourceManagerList();
}

BaseResourceManager* Manager::GetResourceManagerByName(std::string name)
{
	return GetInstance()->_GetResourceManager(name);

}





size_t Manager::GetResourcesToLoadCount() {
#ifdef _NG_USE_RES_THREADS	
	m_res_query_look.lock();
#endif

	size_t size = m_res_queue.size();

#ifdef _NG_USE_RES_THREADS	
	m_res_query_look.unlock();
#endif
	return size;
}

bool Manager::m_busy = false;
unsigned long Manager::m_status = RES_MGR_NONE;

Manager::Manager()
{
	if (m_instance == nullptr)
	{
	
		m_instance = this;
	}


}

Manager::~Manager()
{


	if (m_instance == this)
	{
		m_instance = nullptr;
#ifdef _NG_USE_RES_THREADS
		if (m_res_load_thread != nullptr) {
			m_res_load_thread->detach();
			delete m_res_load_thread;
		}
#endif
	}
};


void Manager::_Flush() {



	TResToLoadQurey new_entry;
	new_entry.m_resource_manager = "";
	new_entry.m_resource_name = "";
	new_entry.m_filename = "";
	new_entry.m_file_flags = 0;
	new_entry.m_resource_flags = 0;
	new_entry.m_xml_file = false;
	new_entry.m_flush = true;



#ifdef _NG_USE_RES_THREADS

	m_res_query_look.lock();
	m_res_queue.push(new_entry);
	m_res_query_look.unlock();

	if (m_res_load_thread != nullptr)
	{

		return;
	}

#else
	m_res_queue.push(new_entry);
#endif




#ifdef _NG_USE_RES_THREADS
	m_res_load_thread = new std::thread(Manager::ResourceDownloadThread, m_opengl_contexed);

#else
	Manager::ResourceDownloadThread(m_opengl_contexed);
#endif





}


void Manager::_FlushIntern() {


	for (std::map<std::string, BaseResourceManager*>::iterator it = m_sub_res_mngr.begin(); it != m_sub_res_mngr.end(); it++)
	{
		SetBits(RES_MGR_FLUSH);
		it->second->FlushIntern();
		UnsetBits(RES_MGR_FLUSH);
		m_status = 0;
	}

}

void Manager::_AddResMngr(BaseResourceManager* mngr)
{

	assert(mngr);

	if (_IsResourceManager(mngr->m_name)) {
		assert(false);
	}
	m_sub_res_mngr.insert(std::pair<std::string, BaseResourceManager*>(mngr->m_name, mngr));


}


BaseResourceManager* Manager::_GetResourceManager(std::string name) {

	std::map<std::string, BaseResourceManager*>::iterator it = m_sub_res_mngr.find(name);
	if (it == m_sub_res_mngr.end()) return nullptr;
	return it->second;
}


std::vector<BaseResourceManager*> Manager::_GetResourceManagerList()
{
	std::vector<BaseResourceManager*> m_out;


	for (std::map<std::string, BaseResourceManager*>::iterator i = m_sub_res_mngr.begin(); i != m_sub_res_mngr.end(); i++)
	{
		m_out.push_back(i->second);
	}

	return m_out;
}


void  Manager::_RemoveResMngr(BaseResourceManager* mngr)
{

	assert(mngr);


	if (!_IsResourceManager(mngr->m_name)) {
		assert(false);
	}

	std::map<std::string, BaseResourceManager* >::iterator p = m_sub_res_mngr.find(mngr->m_name);

	m_sub_res_mngr.erase(p);


}

bool Manager::_IsResourceManager(std::string name)
{

	if (m_sub_res_mngr.find(name) != m_sub_res_mngr.end()) return true;

	return false;
}


unsigned long Manager::_ParseXMLResource(tinyxml2::XMLElement* xmlelm) {

	if (xmlelm == nullptr) {
		return 0;
	}
	std::map<std::string, BaseResourceManager*>::iterator it = m_sub_res_mngr.find(xmlelm->Name());
	if (it != m_sub_res_mngr.end()) {
		it->second->ParseXMLElement(xmlelm);
	}
	unsigned long ret = _ParseXMLResource(xmlelm->NextSiblingElement());

	return ret;

}


// hier habe ich ein ändering gamcht
unsigned long Manager::_ParseXMLElement(tinyxml2::XMLElement* xmlelm) {

	if (xmlelm == nullptr) {
		return 0;
	}

	if (std::string(xmlelm->Name()) == "RESOURCE") {
		unsigned long ret = _ParseXMLResource(xmlelm->FirstChildElement());
		return ret;
	}

	unsigned long ret = _ParseXMLElement(xmlelm->NextSiblingElement());

	return ret;
}

unsigned long Manager::_ParseXML(tinyxml2::XMLDocument* xmldoc) {
	if (!xmldoc) {

		return 2;
	}


	for (tinyxml2::XMLElement* pChild = xmldoc->FirstChildElement(); pChild != 0; pChild = pChild->NextSiblingElement()) {
		unsigned int ret = _ParseXMLElement(pChild);
		if (xmldoc->Error()) {

		}

		if (ret != 0) {



			return ret;
		}
	}
	return 0;
}

unsigned long Manager::_LoadResourceFile(std::string filename) {
	tinyxml2::XMLDocument m_xml;
	if (tinyxml2::XMLError::XML_SUCCESS != m_xml.LoadFile(filename.c_str())) {
		return 1;
	}
	unsigned long err = _ParseXML(&m_xml);

	return err;
}



void   Manager::SetBits(unsigned long bits)
{
	GetInstance()->_SetBits(bits);
}

void   Manager::UnsetBits(unsigned long bits)
{
	GetInstance()->_UnsetBits(bits);
}

unsigned long  Manager::GetStatus()
{
	return GetInstance()->_GetStatus();
}

unsigned long  Manager::_GetStatus()
{
	return m_status;
}


void   Manager::_SetBits(unsigned long bits)
{
#ifdef _NG_USE_RES_THREADS

	m_res_query_look.lock();
#endif
	m_status |= bits;
 #ifdef _NG_USE_RES_THREADS
	m_res_query_look.unlock();
#endif

}

void   Manager::_UnsetBits(unsigned long bits)
{
#ifdef _NG_USE_RES_THREADS

	m_res_query_look.lock();
#endif
	m_status &= ~bits;
#ifdef _NG_USE_RES_THREADS
	m_res_query_look.unlock();
#endif

}




void Manager::ResourceDownloadThread(void* pointer) {
	m_busy = true;
	SetBits(RES_MGR_BUSY);
	TResToLoadQurey entry;

	assert(pointer);



	//->	bool err = gl3wInit() != 0;

	for (;;) {
		m_busy = !m_res_queue.empty();
		while (m_busy != false) {

			m_res_query_look.lock();


			bool empty = m_res_queue.empty();

			if (!empty) {
				entry = m_res_queue.front();
				m_res_queue.pop();
			}

			m_busy = !m_res_queue.empty();

#ifdef _NG_USE_RES_THREADS
			m_res_query_look.unlock();
#endif
			if (empty) break;


			if (entry.m_flush == true) {
				GetInstance()->_FlushIntern();
			}
			else
				if (entry.m_xml_file == false) {


					BaseResourceManager* base = Manager::GetResourceManagerByName(entry.m_resource_manager);


					if (base != nullptr) base->LoadFromFile(entry.m_resource_name, entry.m_resource_name, entry.m_file_flags, entry.m_resource_flags);
				}
				else {
					GetInstance()->_LoadResourceFile(entry.m_filename);

				}

#ifdef _NG_USE_RES_THREADS
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
#endif
		}
		UnsetBits(RES_MGR_BUSY);
	}
	UnsetBits(RES_MGR_BUSY);
	//////////////////////////////////////////////////////////////////////
	/*
	#ifdef _MOGLE_USE_THREADS

		std::this_thread::sleep_for(std::chrono::milliseconds(5000));
		m_res_load_thread->detach();
		delete m_res_load_thread;
		m_res_load_thread = nullptr;
	#endif
	*/
}


bool Manager::Busy() {
#ifdef _NG_USE_RES_THREADS
	return m_busy;
	//return m_res_load_thread != nullptr; 
#else
	return false;
#endif
}



void Manager::LoadResourceFromFile(std::string resource_manager_name, std::string resource_name, std::string filename, unsigned long flags, unsigned long resource_flags)
{


	TResToLoadQurey new_entry;
	new_entry.m_resource_manager = resource_manager_name;
	new_entry.m_resource_name = resource_name;
	new_entry.m_filename = filename;
	new_entry.m_file_flags = flags;
	new_entry.m_resource_flags = resource_flags;
	new_entry.m_xml_file = false;
	new_entry.m_flush = false;



#ifdef _NG_USE_RES_THREADS

	m_res_query_look.lock();
	m_res_queue.push(new_entry);
	m_res_query_look.unlock();

	if (m_res_load_thread != nullptr)
	{
		return;
	}

#else
	m_res_queue.push(new_entry);
#endif




#ifdef _NG_USE_RES_THREADS
	m_res_load_thread = new std::thread(Manager::ResourceDownloadThread, m_opengl_contexed);

#else
	Manager::ResourceDownloadThread(m_opengl_contexed);
#endif

}

unsigned long Manager::ParseXML(tinyxml2::XMLDocument* xmldoc)
{
	return  GetInstance()->_ParseXML(xmldoc);



}





void Manager::AddResMngr(BaseResourceManager* mngr) { GetInstance()->_AddResMngr(mngr); }
void Manager::RemoveResMngr(BaseResourceManager* mngr) { GetInstance()->_RemoveResMngr(mngr); }
Manager* Manager::GetInstance() { return m_instance; }

