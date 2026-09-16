#pragma once

#include <iostream> 
#include <fstream> 
#include <streambuf> 
#include <vector>
#include <assert.h>

namespace ng {
	namespace io {

		class NewCOUTStreambuffer : public std::streambuf
		{
		public:
			NewCOUTStreambuffer() {}
			virtual ~NewCOUTStreambuffer() {}
			inline  std::streambuf* GetCoutBuffer() { return this; }


		protected:
			int overflow(int_type c)
			{
				if (!traits_type::eq_int_type(c, traits_type::eof()))
					onCoutOverflowPrivate(traits_type::to_char_type(c));
				return traits_type::not_eof(c);
			}

			virtual int onCoutOverflowPrivate(char c) { return 0; }
		private:

		};

		class NewCERRStreambuffer : public std::streambuf
		{
		public:
			NewCERRStreambuffer() {}
			virtual ~NewCERRStreambuffer() {}

			inline  std::streambuf* GetCerrBuffer() { return this; }


		protected:
			int overflow(int_type c)
			{
				if (!traits_type::eq_int_type(c, traits_type::eof()))
					onCerrOverflowPrivate(traits_type::to_char_type(c));
				return traits_type::not_eof(c);
			}

			virtual int onCerrOverflowPrivate(char c) { return 0; }
		private:

		};

		class NewCLOGStreambuffer : public std::streambuf
		{
		public:
			NewCLOGStreambuffer() {}
			virtual ~NewCLOGStreambuffer() {}

			inline  std::streambuf* GetClogBuffer() { return this; }


		protected:
			int overflow(int_type c)
			{
				if (!traits_type::eq_int_type(c, traits_type::eof()))
					onClogOverflowPrivate(traits_type::to_char_type(c));
				return traits_type::not_eof(c);
			}

			virtual int onClogOverflowPrivate(char c) { return 0; }
		private:

		};

		class StdOutputBypass :private NewCOUTStreambuffer, NewCERRStreambuffer, NewCLOGStreambuffer
		{
		public:
			StdOutputBypass();
			~StdOutputBypass();
			static inline StdOutputBypass* GetPtr() { return m_ptr; };
		protected:
			virtual int onCoutOverflow(char c);
			virtual int onClogOverflow(char c);
			virtual int onCerrOverflow(char c);


			int onCoutOverflowPrivate(char c);
			int onCerrOverflowPrivate(char c);
			int onClogOverflowPrivate(char c);


		private:
		private:
			void AddBypass(StdOutputBypass *bypass);
			void RemoveBypass(StdOutputBypass *bypass);
			static StdOutputBypass *m_ptr;
			std::vector<StdOutputBypass*> m_bypass;
			typedef std::vector<StdOutputBypass*>::iterator TStdOutIta;
			std::streambuf* old_cerr_buffer;
			std::streambuf* old_clog_buffer;
			std::streambuf* old_cout_buffer;

		};
	}

}