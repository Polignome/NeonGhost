#pragma once

#include "Global.h"
#include <vector>
#include <map>
#include <queue>
#include <string>
#include "CRC.h"
#ifdef _NG_USE_RES_THREADS
#include <thread>
#include <mutex>
#endif
#include "tinyxml2.h"

namespace ng {
	namespace resource {
			template <class T>  // Vordeklarierung für ResPtrContainer
			class Resource;


			class Manager;   // Vordeklarierung für Manager

			template <class T>
			class TemplateResourceManager;

			////////////////7///////////////////////////////////////
			// Attribute für den ResPtrContainer
			////////////////7///////////////////////////////////////

			typedef enum {
				RES_IS_ON_LOAD = 2,  // Die Resource wird geladen
				RES_IS_DUMMY = -1,   // Die Resource ist ein Dummy
				RES_IS_NULL = 0,     // Die Resource ist null (sollte nicht passieren)
				RES_IS_READY = 1     // Die Resurce ist geladen 
			} RESOURCE_STATE;

			enum {
				NO_ATTRIB = 0,          // wird gekillt by flush
				LOCK_RESOURCE = 1,      // wird beim bendendes programms gekillt
				DELETE_WHEN_UNUSED = 2, // wird gekillt es es nicht mehr benutzt wird
				DUMMY_REMOVE = 1 >> 8
			};


			enum {
				RESOURCE_FLAGS = 0,
				RESOURCE_FLAGS_KEEP_SHADER_SOURCE = 1 << 1
			};


			enum {
				RESOURCE_EMPTY_EVENT = 0,
				RESOURCE_CREATE_EVENT = 1,
				RESOURCE_CHANGE_EVENT = 2,
				RESOURCE_DELETE_EVENT = 3
			};

			enum {
				RES_MGR_NONE = 0,
				RES_MGR_BUSY = 1 << 1,
				RES_MGR_FLUSH = 1 << 2,

			};
			///////////////////////////////////////////////////////////////////////////
			//
			// ResPtrContainer ist ein Behälter für die eigendliche resource.
			// Er ermöglicht die Resource während der Laufzeit zu ersetzen
			//
			// Wird eine Resource angefragt die nicht im resourcemanaer ist 
			// wird der coinainer mit einem Dummy gefüllt. Wird diese resource wäred 
			// der Lufzeit geladen wird der Dummy im Cointainer ersetzt
			//////////////////////////////////////////////////////////////////////////





			template <class T>
			class ResPtrContainer {
			public:
				// Constructor
				ResPtrContainer(T* res_ptr, RESOURCE_STATE state = RES_IS_READY) :m_res_ptr(res_ptr), m_state(state) {}

				// Destructor
				~ResPtrContainer() {
				}

				void GetChangeSignal() {}



			protected:
			private:
				// Erlauben, dass Resource vollen Zugriff auf alle Methoden hat
				friend Resource<T>;
				friend TemplateResourceManager<T>;

				// Pointer auf die Resource  ausgeben
				T* GetPtr() { return m_res_ptr; }
				void DeletePtr() { if (m_res_ptr != nullptr) delete m_res_ptr; }
				// Neuen Pointer setetzen ung gegebenfals löschen
				void ReplacePtr(T* new_res_ptr, bool deleteold = true) {
					bool changed = false;
					if (m_res_ptr != nullptr) changed = true;
					if (changed && deleteold) { delete m_res_ptr; }
					m_res_ptr = new_res_ptr;

				}
				// Privater Constructor
				ResPtrContainer() :m_res_ptr(nullptr), m_state(RES_IS_NULL) {}

				T* m_res_ptr;              // Pointer auf resource
				RESOURCE_STATE m_state;    // Status der resource



			};


			///////////////////////////////////////////////////////////////////////////
			//
			// Dies ist der eigendliche resource container
			// Im Kern ist ein shared_ptr 
			//
			///////////////////////////////////////////////////////////////////////////




			template <class T>
			class Resource :public std::shared_ptr<ResPtrContainer<T>> {
			public:

				// Constructor für Lehre Resource 
				Resource(void) :std::shared_ptr<ResPtrContainer<T>>(), m_resourcedata_changed(false)
				{
					res_event(RESOURCE_EMPTY_EVENT);
				}

				~Resource(void) {




				}
				//Liefert Resource zurück
				T& operator *() { return *this->get()->GetPtr(); }





				Resource& operator = (const Resource& res)
				{

					std::shared_ptr < ResPtrContainer<T>> r = res;
					this->swap(r);
					return *this;
				}

				//Liefert einen Pointer auf Resource zurück
				T* operator -> () { return this->get()->GetPtr(); }
			
				const T* operator->(void) const { return this->get()->GetPtr(); }
			
				// PRIVAT Liefert die Anzahl der aktiven Benutzer zurück    
				long GetUseCount() { return this->use_count(); }

				// Lifert true wenn pointer == nullptr
				bool isNull() { return this->get() == nullptr; }

				bool hasDataChanged(bool reset_flag = true)
				{
					bool temp = m_resourcedata_changed;
					if (reset_flag) m_resourcedata_changed = false;
					return temp;

				}

			protected:

			private:

				void changed_event() {
					m_resourcedata_changed = true;
				}


				friend ResPtrContainer<T>;
				friend TemplateResourceManager<T>;
				friend Resource<T>;

				bool m_resourcedata_changed;


				void res_event(unsigned char i) {
					if (this->get() == nullptr) return;


				}

				// PRIVAT Einfacher Constructor
				// Kann nur vom resourcemannger genutzt werden
				Resource(ResPtrContainer<T>* res) :std::shared_ptr<ResPtrContainer<T>>(res) {

					//-->res->m_changed.connect(this, &Resource::changed_event);

				//	if (res != nullptr) res->m_event.connect(&Resource<T>::res_event, this);



				}

				// PRIVAT Constructor mit deleter
				// Kann nur vom resourcemannger genutzt werden
				Resource(ResPtrContainer<T>* res, void (*deleter)(ResPtrContainer<T>*)) :std::shared_ptr<ResPtrContainer<T>>(res, deleter)
				{
					//-->res->m_changed.connect(this, &Resource::changed_event);


				//	if (res != nullptr) res->m_event.connect(&Resource<T>::res_event, this);

				}


				int i;
			};




			////////////////////////////////////////////////////////////////////////////////////////////////////////
			//                                                                                                    //
			// Base-Resource-Manger                                                                               //
			// -------------------------------------------------------------------------------------------------  //
			// Diese Klasse stellt die Schnitstelle zwischen dem template Manager und dem MainManager da          //                                                                                                 
			//                                                                                                    //
			//                                                                                                    //
			////////////////////////////////////////////////////////////////////////////////////////////////////////

			class BaseResourceManager {
			public:
				// Constructor 
				// name = der Name mit dem der Manager im MainManager eigehängt wird
				BaseResourceManager(std::string name);
				// Liefert den Name des manager zurück
				inline std::string GetName() { return m_name; }
				// Liefert eine Liste mit dem Namen aller Resourcen im Manager zurück
				virtual std::vector<std::string> GetResourceNameList() { std::vector<std::string> nop; return nop; }

			protected:
				// Hängt den Manager im mainmanaer ein
				void RegisterMe();

				// Löst den Manager aus dem Mainmanager 
				void RemoveMe();

				// Löscht alle nicht mehr benötigten resourcen im Manager
				virtual void FlushIntern() {};

				// Resource wird aus XML gelesen 
				virtual unsigned long ParseXMLElement(tinyxml2::XMLElement* element) { return 0; }

				// Resource wird aus einer Datei gelden
				virtual void LoadFromFile(std::string res_name, std::string filename, unsigned long file_flags, unsigned long res_flags) {}

			private:
				friend Manager;
				std::string m_name;

			};


			////////////////////////////////////////////////////////////////////////////////////////////////////////
			//                                                                                                    //
			// Manager                                                                                            //
			// -------------------------------------------------------------------------------------------------  //
			//                                                                                                    //
			//                                                                                                    //
			//                                                                                                    //
			////////////////////////////////////////////////////////////////////////////////////////////////////////

			class Manager {
			public:

				// Constructor
				Manager();

				// Destructor
				~Manager();

				static std::string GetResourcePath();
				static void SetResourcePath(std::string path);

				static void SetOpenGlContexed(void* contexed);

				// In name: name des 
				// Ermittelt ob ein resourcemanger berteits in den Mainresmanager eingehaengt worden ist
				static bool IsResourceManager(std::string name);

				// Loescht alle nicht mehr benoetigten resourcen aus den Speicher
				static void Flush();

				// in filename = filename des resourcefiles
				static unsigned long LoadResourceFile(std::string filename);

				// Unsericher !!
				static std::vector<BaseResourceManager*> GetResourceManagerList();

				// Liefert eine Pointer auf eine BaseResourceManger zurueck.
				// nullptr wenn der Manager nicht vorhanden ist
				static BaseResourceManager* GetResourceManagerByName(std::string name);

				// Laed eine Resource in den Resourcemanager
				// resource_manager_name : Name des Manager z.B. bei Bitmaps "MOGLE_BMP_MANAGER
				// resource_name         : Name der Resource z.B. bei Bitmaps "WALL_NR_!"
				// filename              : Name des Quellfiles z.B. bei Bitmaps ../textures/wall1.bmp
				// flags                 : Ist von Manager zu Manager unterschiedlich
				// resource_flags        : NO_ATTRIB , LOCK_RESOURCE ,DELETE_WHEN_UNUSED u.s.w, 
				static void LoadResourceFromFile(std::string resource_manager_name, std::string resource_name, std::string filename, unsigned long flags = 0, unsigned long resource_flags = DELETE_WHEN_UNUSED);

				// Gibt aus wieviele Resourcen im noch Thread warten bis sie geladen werden können 
				static size_t GetResourcesToLoadCount();

				// Ist true wenn der thread noch active ist
				static bool Busy();
				static unsigned long GetStatus();
				static void LockMutex();
				static void UnlockMutex();
			protected:

			private:

				static void SetBits(unsigned long bits);
				void _SetBits(unsigned long bits);
				static void UnsetBits(unsigned long bits);
				void _UnsetBits(unsigned long bits);
				unsigned long _GetStatus();


				static std::string m_resource_path;

				// Informationen der im tread zu ladenen resourcen
				typedef struct {
					std::string m_resource_name;     // Name der Reosurce
					std::string m_resource_manager;  // Name des Resourcemanagers
					std::string m_filename;          // Datename der resource
					std::string m_file_parameter;    // Fileparameter ....
					bool m_xml_file;                 // XML-File
					bool m_flush;
					unsigned long m_file_flags;      // Div. Resourcemanager spezifische flags
					unsigned long m_resource_flags;  // Resourceflags
				} TResToLoadQurey;


				friend BaseResourceManager;  // Der BaseresourceManager hat zugruiff auf alle methoden und Variablen

				static std::queue<TResToLoadQurey> m_res_queue;  // Stack mit allen noch zu ladenen Resourcen die noch vom thread geladen werden müssen
				static void* m_opengl_contexed;

#ifdef _NG_USE_RES_THREADS

				static std::mutex m_res_query_look;
				static std::atomic_bool m_res_query_ready_to_read_or_write;
				static std::thread* m_res_load_thread; // Main Thread
#endif
				std::vector<BaseResourceManager*> _GetResourceManagerList();  // Liste aller resourcemanager
				BaseResourceManager* _GetResourceManager(std::string name);   // Liefert einen Resourcemanager zurück
				static Manager* m_instance;                                   // Instance 
				std::map<std::string, BaseResourceManager*> m_sub_res_mngr;   // Liste aller eingehängten Resourcemanger


				static unsigned long ParseXML(tinyxml2::XMLDocument* xmldoc); // Liest ein xml ein
				static void AddResMngr(BaseResourceManager* mngr);            // Fühgt einen resourcemanger zu 
				static void RemoveResMngr(BaseResourceManager* mngr);
				static Manager* GetInstance();
				static void ResourceDownloadThread(void* pointer);

				void _AddResMngr(BaseResourceManager* mngr);
				bool _IsResourceManager(std::string name);
				void _RemoveResMngr(BaseResourceManager* mngr);
				void _Flush();
				void _FlushIntern();

				unsigned long _ParseXML(tinyxml2::XMLDocument* xmldoc);
				unsigned long _ParseXMLElement(tinyxml2::XMLElement* xmlelm);
				unsigned long _ParseXMLResource(tinyxml2::XMLElement* xmlelm);
				unsigned long _LoadResourceFile(std::string filename);

				static std::string _GetResourcePath();
				static void _SetResourcePath(std::string path);
				static unsigned long m_status;
				static bool m_busy;

			};




			//static Manager m_main_res_manager;





			template <class T>
			class TemplateResourceManager :public BaseResourceManager {
			public:

				// Liefert die Namen aller resourcen im manager zurück
				static std::vector<std::string>  GetResourceNames() {
					return GetInstance()->GetResourceNameList();
				}




				TemplateResourceManager() {
					assert(0 == 1);
				}
				// Constructor 
				// Name: Setzt den Namem des Resourcemanaer
				TemplateResourceManager(std::string name) :m_remove_if_zerro(false), BaseResourceManager(name), m_flags(0)
				{

					if (m_instance == nullptr) {

						int str_len = int(((float)std::rand() / (float)RAND_MAX) * (255.0f / 4));
#ifdef _M_X64
						m_dummy_str = "__DUMMY_RESOURE__" + std::to_string((uint64_t)this);
#else
						m_dummy_str = "__DUMMY_RESOURE__" + std::to_string((unsigned long)this);
#endif					
						m_instance = this;
						RegisterMe();
					}

				}

				// Destructor
				virtual ~TemplateResourceManager() {

					if (m_instance != this) {

						return;
					}

					_Flush(true);
					m_instance = nullptr;
					RemoveMe();
					_ViewInfo();


				}


				// Ist noch nicht def
				virtual void* GetResourceManagerInfo() { return nullptr; }



				static unsigned long ParseXML(tinyxml2::XMLElement* element) { return GetInstance()->ParseXMLElement(element); }

				static  Resource<T> GetResource(const std::string& name, bool do_not_delete_at_destroy = false) {

					uint64_t hash = tools::CRC::CRC64(name.c_str());
					return GetInstance()->_GetResource(name, hash, do_not_delete_at_destroy);

				}

				static  Resource<ResPtrContainer<T>> GetResource(uint64_t hash, bool do_not_delete_at_destroy = false) {
					return GetInstance()->_GetResource("", hash, do_not_delete_at_destroy);
				}





				/*****************************************************/
				/*                                                   */
				/*****************************************************/
				static void Flush() {

					GetInstance()->_Flush(false);
				}

				/*****************************************************/
				/*                                                   */
				/*****************************************************/
				static bool IsResourceInManager(std::string name) {
					return GetInstance()->_IsResourceInManager(name);
				}

				/*****************************************************/
				/*                                                   */
				/*****************************************************/
				static void SetFlags(unsigned long flags) { GetInstance()->_SetFlags(flags); }

				/*****************************************************/
				/*                                                   */
				/*****************************************************/
				static unsigned long GetFlags(void) { return GetInstance()->_GetFlags(); }

				static std::string GetNameByID(uint64_t id)
				{
					return GetInstance()->_GetNameByID(id);
				}

				static std::string GetDummyResourceName() { return GetInstance()->_GetDummyResourceName(); }


			public:

				virtual std::vector<std::string> GetResourceNameList()
				{
					std::vector<std::string> nop;

					for (std::map<uint64_t, std::string>::iterator i = m_hash_string.begin(); i != m_hash_string.end(); i++)
					{
						nop.push_back(i->second);
					}


					return nop;
				}

				static void ViewInfo() {
					GetInstance()->_ViewInfo();
				}

			protected:


				void SwapResource(ResPtrContainer<T>* target, T* new_res, bool delete_old = true) {
					GetInstance()->_SwapResource(target, new_res, delete_old);
				}


				std::string GetResName(ResPtrContainer<T>* target) {
					typename std::map<ResPtrContainer<T>*, uint64_t>::iterator it2 = m_sptr_hash.find(target);
					assert(it2 != m_sptr_hash.end());

					typename std::map<uint64_t, std::string>::iterator it3 = m_hash_string.find(it2->second);
					assert(it3 != m_hash_string.end());

					return it3->second;
				}


				void _SwapResource(ResPtrContainer<T>* target, T* new_res, bool delete_old = true) {

					assert(target);
					assert(new_res);

					typename std::map<ResPtrContainer<T>*, T*>::iterator ita = m_con_ptr.find(target);
					assert(ita != m_con_ptr.end());



					bool changed = false;

					if (ita->second != m_global_dummy_resource) {
						typename std::map<T*, ResPtrContainer<T>*>::iterator itb = m_ptr_con.find(target->GetPtr());
						assert(target->GetPtr());
						assert(itb != m_ptr_con.end());

						{
							typename std::map<T*, ResPtrContainer<T>*>::iterator itb = m_ptr_con.find(new_res);
							assert(itb == m_ptr_con.end());

						}

						m_ptr_con.erase(itb);
					}


					if (target->GetPtr() == m_global_dummy_resource) {

						m_dummy_in_used_counter--;
						target->ReplacePtr(new_res, false);
						changed = true;
						assert(m_dummy_in_used_counter >= 0);

						if (m_dummy_in_used_counter <= 0) {

							if (m_global_dummy_resource != nullptr) {
								delete m_global_dummy_resource;
							}
							m_global_dummy_resource = nullptr;
							m_dummy_in_used_counter = 0;
						}
					}
					else {
						target->ReplacePtr(new_res, delete_old);
						changed = true;
						//					if (new_res != m_global_dummy_resource) {
						//					}

					}



					m_ptr_con.insert(std::pair<T*, ResPtrContainer<T>*>(new_res, target));
					m_con_ptr.erase(ita);
					m_con_ptr.insert(std::pair<ResPtrContainer<T>*, T*>(target, new_res));

					if (changed) {
						//-->target->m_changed();
					}

				}



				virtual void LoadFromFile(std::string res_name, std::string filename, unsigned long file_flags, unsigned long res_flags) {
				}

				/*****************************************************/
				/*                                                   */
				/*****************************************************/
				virtual T* CreateDummyResourceEvent() {


					return nullptr;//new T();
				}


				static std::string m_dummy_str;

				/*****************************************************/
				/*                                                   */
				/*****************************************************/
				static uint64_t AddResource(const std::string& name, T* resource, unsigned long attrib = DELETE_WHEN_UNUSED) {
					return GetInstance()->_AddResource(name, resource, attrib);
				}

				/*****************************************************/
				/*                                                   */
				/*****************************************************/
				void FlushIntern() {
					_Flush();
				}
			protected:
				/// Nicht benutzen nur für RenderInGui wenn es unbedingt sein muss
				T* GetWeakResPtr(const std::string& name)
				{
					uint64_t hash_code = tools::CRC::CRC64(name.c_str());
					typename std::map<uint64_t, T*>::iterator it = m_hash_sptr.find(hash_code);
					if (it == m_hash_sptr.end()) return nullptr;
					return it->second;
				}
			private:
				unsigned long m_flags;


				static TemplateResourceManager<T>* m_instance;
				static TemplateResourceManager<T>* GetInstance() { assert(m_instance); return m_instance; }

				bool m_remove_if_zerro;

				typedef struct {
					long m_used;
					unsigned long m_attrib;
				}TResInfo;

				inline void _SetFlags(unsigned long flags) { m_flags = flags; }
				inline unsigned long _GetFlags(void) { return m_flags; }



				std::map<T*, ResPtrContainer<T>*> m_ptr_con;
				std::map<ResPtrContainer<T>*, T*> m_con_ptr;
				std::map<uint64_t, ResPtrContainer<T>*> m_hash_sptr;
				std::map<ResPtrContainer<T>*, uint64_t> m_sptr_hash;
				std::map<uint64_t, std::string> m_hash_string;
				std::map<ResPtrContainer<T>*, TResInfo> m_uses;


				static std::string _GetNameByID(uint64_t id)
				{
					return std::string("");

					if (auto search = m_hash_string.find(id); search != m_hash_string.end())
					{
						return search->second;
					}
					return std::string("");
				}

				void _ViewInfo() {

				}

				static T* m_global_dummy_resource;
				static unsigned long m_dummy_in_used_counter;

				inline std::string _GetDummyResourceName() { return m_dummy_str; }

				/*****************************************************/
				/*                                                   */
				/*****************************************************/
				inline ResPtrContainer<T>* _IsResourceInMannager(ResPtrContainer<T>* resource) {
					if (m_uses.find(resource) != m_uses.end()) return resource;
					return nullptr;
				}

				inline T* _IsResourceInMannager(T* resource) {
					if (m_ptr_con.find(resource) != m_ptr_con.end()) return resource;
					return nullptr;
				}

				/*****************************************************/
				/*                                                   */
				/*****************************************************/
				inline ResPtrContainer<T>* _IsResourceInMannager(uint64_t hash) {

					typename std::map<uint64_t, ResPtrContainer<T>*>::iterator it = m_hash_sptr.find(hash);
					if (it == m_hash_sptr.end()) return nullptr;
					return _IsResourceInMannager(it->second);
				}

				/*****************************************************/
				/*                                                   */
				/*****************************************************/
				inline bool _IsResourceInManager(std::string name) {
					uint64_t hash_code = tools::CRC::CRC64(name.c_str());
					typename std::map<uint64_t, ResPtrContainer<T>*>::iterator it = m_hash_sptr.find(hash_code);
					if (it == m_hash_sptr.end()) return false;
					return true;

				}


				/*****************************************************/
				/*                                                   */
				/*****************************************************/

				inline uint64_t _AddResource(
					const std::string& name,
					T* resource,
					unsigned long attrib)
				{
					// ------------------------------------------------------------
					// 1. Namen hashen
					// ------------------------------------------------------------

					printf("ADD RESOURCE\n manager = %s\nname  =%s\ntype   =%s\n", GetName().c_str(), name.c_str(), typeid(T).name());

					const uint64_t hash_code =
						tools::CRC::CRC64(name.c_str());

					// ------------------------------------------------------------
					// 2. Prüfen, ob bereits eine Resource mit diesem Namen existiert
					// ------------------------------------------------------------

					ResPtrContainer<T>* ptr =
						_IsResourceInMannager(hash_code);

					// ------------------------------------------------------------
					// 3. Falls ein echter Pointer übergeben wurde:
					//
					//    Dieser Pointer darf noch nicht in unserem Manager
					//    registriert sein.
					// ------------------------------------------------------------

					if (resource != nullptr)
					{
						auto ptr_it = m_ptr_con.find(resource);

						// Eine Resource darf nicht zweimal registriert werden.
						assert(ptr_it == m_ptr_con.end());

						// Optional zusätzlich prüfen, ob sie bereits als
						// Container irgendwo eingetragen ist.
						//
						// Das ist normalerweise nicht notwendig, kann aber
						// beim Debuggen sehr hilfreich sein.
						for (const auto& entry : m_con_ptr)
						{
							assert(entry.second != resource);
						}
					}

					// ------------------------------------------------------------
					// 4. Resource existiert bereits
					//
					//    In diesem Fall ersetzen wir die bisherige Resource.
					// ------------------------------------------------------------

					if (ptr != nullptr)
					{
						// Nur Debug-Check:
						// Der Hash sollte bereits einen Namen besitzen.
						auto name_it = m_hash_string.find(hash_code);

						assert(name_it != m_hash_string.end());

						// Die vorhandene Resource durch die neue ersetzen.
						//
						// SwapResource() kümmert sich um:
						//   - alten Pointer
						//   - m_ptr_con
						//   - m_con_ptr
						//   - Dummy-Resource
						//   - Container-Zuordnung
						//
						_SwapResource(ptr, resource, true);

						return hash_code;
					}

					// ------------------------------------------------------------
					// 5. Neue Resource
					// ------------------------------------------------------------

					T* res = resource;

					// ------------------------------------------------------------
					// 6. Keine Resource übergeben:
					//
					//    Dummy-Resource verwenden.
					// ------------------------------------------------------------

					if (res == nullptr)
					{
						if (m_global_dummy_resource == nullptr)
						{
							m_global_dummy_resource =
								CreateDummyResourceEvent();
						}

						res = m_global_dummy_resource;

						++m_dummy_in_used_counter;
					}

					// ------------------------------------------------------------
					// 7. Resource-Informationen erzeugen
					// ------------------------------------------------------------

					TResInfo ri;

					ri.m_used = 0;
					ri.m_attrib = attrib;

					// ------------------------------------------------------------
					// 8. Resource-Container erzeugen
					// ------------------------------------------------------------

					ResPtrContainer<T>* rescon =
						new ResPtrContainer<T>(res);

					assert(rescon != nullptr);

					// ------------------------------------------------------------
					// 9. Pointer -> Container
					//
					//    Dummy-Resource wird NICHT eingetragen, weil dieselbe
					//    Dummy-Instanz von mehreren Containern benutzt werden kann.
					// ------------------------------------------------------------

					if (res != m_global_dummy_resource)
					{
						auto result =
							m_ptr_con.emplace(res, rescon);

						assert(result.second);
					}

					// ------------------------------------------------------------
					// 10. Container -> Pointer
					// ------------------------------------------------------------

					{
						auto result =
							m_con_ptr.emplace(rescon, res);

						assert(result.second);
					}

					// ------------------------------------------------------------
					// 11. Nutzungsinformationen
					// ------------------------------------------------------------

					{
						auto result =
							m_uses.emplace(rescon, ri);

						assert(result.second);
					}

					// ------------------------------------------------------------
					// 12. Hash -> Container
					// ------------------------------------------------------------

					{
						auto result =
							m_hash_sptr.emplace(hash_code, rescon);

						assert(result.second);
					}

					// ------------------------------------------------------------
					// 13. Container -> Hash
					// ------------------------------------------------------------

					{
						auto result =
							m_sptr_hash.emplace(rescon, hash_code);

						assert(result.second);
					}

					// ------------------------------------------------------------
					// 14. Hash -> Name
					// ------------------------------------------------------------

					{
						auto result =
							m_hash_string.emplace(hash_code, name);

						assert(result.second);
					}

					// ------------------------------------------------------------
					// 15. Debug-Checks
					// ------------------------------------------------------------

					assert(m_hash_sptr.find(hash_code) != m_hash_sptr.end());
					assert(m_sptr_hash.find(rescon) != m_sptr_hash.end());
					assert(m_hash_string.find(hash_code) != m_hash_string.end());
					assert(m_con_ptr.find(rescon) != m_con_ptr.end());
					assert(m_uses.find(rescon) != m_uses.end());

					if (res != m_global_dummy_resource)
					{
						assert(m_ptr_con.find(res) != m_ptr_con.end());
					}

					return hash_code;
				}


				
				
				/*****************************************************/
				/*                                                   */
				/*****************************************************/
				static void _DeletePtr(ResPtrContainer<T>* t) {
					assert(m_instance);
					m_instance->_dec(t, true);

				}
				/*****************************************************/
				/*                                                   */
				/*****************************************************/
				static void _DeletePtrNoDelByZero(ResPtrContainer<T>* t) {
					assert(m_instance);
					m_instance->_dec(t, false);

				}



				/*****************************************************/
				/*                                                   */
				/*****************************************************/
				inline void _inc(ResPtrContainer<T>* t)
				{

					typename std::map<ResPtrContainer<T>*, TResInfo>::iterator it = m_uses.find(t);
					assert(it != m_uses.end());
					it->second.m_used++;
					//TLOG(_getResName(t) << " usesed by " << it->second.m_used);


				}

				/*****************************************************/
				/*                                                   */
				/*****************************************************/

				inline std::string _getResName(ResPtrContainer<T>* t) {
					typename std::map<ResPtrContainer<T>*, uint64_t>::iterator it2 = m_sptr_hash.find(t);
					assert(it2 != m_sptr_hash.end());

					typename std::map<uint64_t, ResPtrContainer<T>*>::iterator it1 = m_hash_sptr.find(it2->second);
					assert(it1 != m_hash_sptr.end());

					typename std::map<uint64_t, std::string>::iterator it3 = m_hash_string.find(it2->second);
					assert(it3 != m_hash_string.end());
					return it3->second;
				}

				/*****************************************************/
				/*                                                   */
				/*****************************************************/
				inline void _dec(ResPtrContainer<T>* t, bool deletebyzero)
				{
					if (t == nullptr) return;
					typename std::map<ResPtrContainer<T>*, TResInfo>::iterator it = m_uses.find(t);
					assert(it != m_uses.end());
					it->second.m_used--;


					assert(it->second.m_used >= 0);


					if (it->second.m_used == 0 && it->second.m_attrib & DELETE_WHEN_UNUSED && deletebyzero == true)
					{

						typename std::map<ResPtrContainer<T>*, T*>::iterator ita = m_con_ptr.find(t);
						assert(ita != m_con_ptr.end());

						if (ita->second != m_global_dummy_resource) {


							typename std::map<T*, ResPtrContainer<T>*>::iterator itb = m_ptr_con.find(t->GetPtr());
							assert(itb != m_ptr_con.end());
							m_ptr_con.erase(itb);

						}


						//TLOG("Die resource wird laut attrib nicht mehr gebraucht und daher gelöscht");
						typename std::map<ResPtrContainer<T>*, uint64_t>::iterator it2 = m_sptr_hash.find(t);
						assert(it2 != m_sptr_hash.end());

						m_uses.erase(it);
						typename std::map<uint64_t, ResPtrContainer<T>*>::iterator it1 = m_hash_sptr.find(it2->second);
						assert(it1 != m_hash_sptr.end());

						typename std::map<uint64_t, std::string>::iterator it3 = m_hash_string.find(it2->second);
						assert(it3 != m_hash_string.end());




						m_con_ptr.erase(ita);

						m_hash_sptr.erase(it1);
						m_sptr_hash.erase(it2);
						m_hash_string.erase(it3);


						std::map<ResPtrContainer<T>*, T*> m_con_ptr;
						if (t->GetPtr() != m_global_dummy_resource) t->DeletePtr();
						else {
							m_dummy_in_used_counter--;
							assert(m_dummy_in_used_counter >= 0);
							if (m_dummy_in_used_counter == 0)
							{

								delete m_global_dummy_resource;
								m_global_dummy_resource = nullptr;

							}

						}

						delete t;


					}


				}


				/*****************************************************/
				/*                                                   */
				/*****************************************************/
				void _EnableDelIfNull(bool a) {


					if (a == m_remove_if_zerro) {
						return;
					}
					m_remove_if_zerro = a;
					if (m_remove_if_zerro) _Flush(true);


				}


				/*****************************************************/
				/*                                                   */
				/*****************************************************/
				void _Flush(bool final_flush = false)
				{


					std::vector<ResPtrContainer<T>*> temp;
					for (typename std::map<ResPtrContainer<T>*, TResInfo>::iterator it = m_uses.begin(); it != m_uses.end(); it++) {
						if (it->second.m_used == 0) {
							if (final_flush && (it->second.m_attrib & LOCK_RESOURCE)) // Bei final_flush wird LOCK_RESOURCE übergangen!
							{

								assert(it->second.m_used == 0);
								temp.push_back(it->first);
							}
							else temp.push_back(it->first);
						}
					}
					for (unsigned int i = 0; i < temp.size(); i++) _Remove(temp[i]);




				}

				/*****************************************************/
				/*                                                   */
				/*****************************************************/
				void _Remove(ResPtrContainer<T>* t) {

					typename std::map<ResPtrContainer<T>*, T*>::iterator ita = m_con_ptr.find(t);
					assert(ita != m_con_ptr.end());

					if (ita->second != m_global_dummy_resource) {

						typename std::map<T*, ResPtrContainer<T>*>::iterator itb = m_ptr_con.find(t->GetPtr());
						assert(itb != m_ptr_con.end());
						m_ptr_con.erase(itb);
					}

					typename std::map<ResPtrContainer<T>*, TResInfo>::iterator it = m_uses.find(t);
					assert(it != m_uses.end());
					typename std::map<ResPtrContainer<T>*, uint64_t>::iterator it2 = m_sptr_hash.find(t);
					assert(it2 != m_sptr_hash.end());
					typename std::map<uint64_t, ResPtrContainer<T>*>::iterator it1 = m_hash_sptr.find(it2->second);
					assert(it1 != m_hash_sptr.end());
					std::map<uint64_t, std::string>::iterator it3 = m_hash_string.find(it2->second);
					assert(it3 != m_hash_string.end());




					m_con_ptr.erase(ita);


					m_hash_sptr.erase(it1);
					m_sptr_hash.erase(it2);
					m_hash_string.erase(it3);
					m_uses.erase(it);


					if (t->GetPtr() != m_global_dummy_resource) t->DeletePtr();
					else {
						m_dummy_in_used_counter--;
						assert(m_dummy_in_used_counter >= 0);
						if (m_dummy_in_used_counter == 0)
						{
							delete m_global_dummy_resource;
							m_global_dummy_resource = nullptr;

						}

					}




					delete t;

				}

				/*****************************************************/
				/*                                                   */
				/*****************************************************/
				static void DeletePtr(ResPtrContainer<T>* t) {
					GetInstance()->_DeletePtr(t);
				}

				/*****************************************************/
				/*                                                   */
				/*****************************************************/
				Resource<T> _GetResource(std::string name, uint64_t hash, bool do_not_delete_at_destroy = false) {
					ResPtrContainer<T>* res = _IsResourceInMannager(hash);



					if (res == nullptr) {



						if (m_dummy_str == "") {
#ifdef _M_X64
							m_dummy_str = "__DUMMY_RESOURE__" + std::to_string((uint64_t)this);
#else
							m_dummy_str = "__DUMMY_RESOURE__" + std::to_string((unsigned long)this);
#endif					

						}

						res = _IsResourceInMannager(tools::CRC::CRC64(m_dummy_str));


						if (res == nullptr) {
							uint64_t hc = _AddResource(name, nullptr, 0);
							hash = hc;
							res = _IsResourceInMannager(hc);
						}
						assert(res); // muss noch geändert werden



					}




					_inc(res);

					if (do_not_delete_at_destroy) return  Resource<T>(res, TemplateResourceManager<T>::_DeletePtrNoDelByZero);
					return Resource<T>(res, TemplateResourceManager<T>::_DeletePtr);
				}

			};

			template <class T>
			TemplateResourceManager<T>* TemplateResourceManager<T>::m_instance = nullptr;

			template <class T>
			std::string TemplateResourceManager<T>::m_dummy_str = "";

			template <class T>
			T* TemplateResourceManager<T>::m_global_dummy_resource = nullptr;

			template <class T>
			unsigned long TemplateResourceManager<T>::m_dummy_in_used_counter = 0;

	}
}