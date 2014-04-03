/******************************************************************************

CAJUN: a C++ API for the JSON object interchange format

Copyright (c) 2009-2010, Terry Caton
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright 
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the projecct nor the names of its contributors 
      may be used to endorse or promote products derived from this software 
      without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/

#pragma once

#include <cassert>
#include <algorithm>
#include <map>
#include <deque>
#include <list>
#include <string>
#include <stdexcept>
#include <iostream>
#include <vector>
#include <set>
#include <sstream>
#include <iomanip>


/*  

TODO:
* better documentation (doxygen?)
* Unicode support
* parent element accessors

*/

namespace json
{

namespace Version
{
   enum { MAJOR = 2 };
   enum { MINOR = 0 };
   enum {ENGINEERING = 2 };
}

/////////////////////////////////////////////////
// forward declarations (more info further below)


class Visitor;
class ConstVisitor;

template <typename ValueTypeT>
class TrivialType_T;

typedef TrivialType_T<double> Number;
typedef TrivialType_T<bool> Boolean;
typedef TrivialType_T<std::string> String;

class Object;
class Array;
class Null;



/////////////////////////////////////////////////////////////////////////
// Exception - base class for all JSON-related runtime errors

class Exception : public std::runtime_error
{
public:
   Exception(const std::string& sMessage);
};




/////////////////////////////////////////////////////////////////////////
// UnknownElement - provides a typesafe surrogate for any of the JSON-
//  sanctioned element types. This class allows the Array and Object
//  class to effectively contain a heterogeneous set of child elements.
// The cast operators provide convenient implicit downcasting, while
//  preserving dynamic type safety by throwing an exception during a
//  a bad cast. 
// The object & array element index operators (operators [std::string]
//  and [size_t]) provide convenient, quick access to child elements.
//  They are a logical extension of the cast operators. These child
//  element accesses can be chained together, allowing the following
//  (when document structure is well-known):
//  String str = objInvoices[1]["Customer"]["Company"];


class UnknownElement
{
public:
   UnknownElement();
   UnknownElement(const UnknownElement& unknown);
   UnknownElement(const Object& object);
   UnknownElement(const Array& array);
   UnknownElement(const Number& number);
   UnknownElement(const Boolean& boolean);
   UnknownElement(const String& string);
   UnknownElement(const Null& null);

   ~UnknownElement();

   UnknownElement& operator = (const UnknownElement& unknown);

   // implicit cast to actual element type. throws on failure
   operator const Object& () const;
   operator const Array& () const;
   operator const Number& () const;
   operator const Boolean& () const;
   operator const String& () const;
   operator const Null& () const;

   // implicit cast to actual element type. *converts* on failure, and always returns success
   operator Object& ();
   operator Array& ();
   operator Number& ();
   operator Boolean& ();
   operator String& ();
   operator Null& ();

   // provides quick access to children when real element type is object
   UnknownElement& operator[] (const std::string& key);
   const UnknownElement& operator[] (const std::string& key) const;

   // provides quick access to children when real element type is array
   UnknownElement& operator[] (size_t index);
   const UnknownElement& operator[] (size_t index) const;

   // implements visitor pattern
   void Accept(ConstVisitor& visitor) const;
   void Accept(Visitor& visitor);

   // tests equality. first checks type, then value if possible
   bool operator == (const UnknownElement& element) const;

   // check if this is a certain type
   template <typename ElementTypeT>
   bool IsOfType() const;

private:
   class Imp;

   template <typename ElementTypeT>
   class Imp_T;

   class CastVisitor;
   class ConstCastVisitor;
   
   template <typename ElementTypeT>
   class CastVisitor_T;

   template <typename ElementTypeT>
   class ConstCastVisitor_T;

   template <typename ElementTypeT>
   const ElementTypeT& CastTo() const;

   template <typename ElementTypeT>
   ElementTypeT& ConvertTo();

   Imp* m_pImp;
};


/////////////////////////////////////////////////////////////////////////////////
// Array - mimics std::deque<UnknownElement>. The array contents are effectively 
//  heterogeneous thanks to the ElementUnknown class. push_back has been replaced 
//  by more generic insert functions.

class Array
{
public:
   typedef std::deque<UnknownElement> Elements;
   typedef Elements::iterator iterator;
   typedef Elements::const_iterator const_iterator;

   iterator Begin();
   iterator End();
   const_iterator Begin() const;
   const_iterator End() const;
   
   iterator Insert(const UnknownElement& element, iterator itWhere);
   iterator Insert(const UnknownElement& element);
   iterator Erase(iterator itWhere);
   void Resize(size_t newSize);
   void Clear();

   size_t Size() const;
   bool Empty() const;

   UnknownElement& operator[] (size_t index);
   const UnknownElement& operator[] (size_t index) const;

   bool operator == (const Array& array) const;

private:
   Elements m_Elements;
};


/////////////////////////////////////////////////////////////////////////////////
// Object - mimics std::map<std::string, UnknownElement>. The member value 
//  contents are effectively heterogeneous thanks to the UnknownElement class

class Object
{
public:
   struct Member {
      Member(const std::string& nameIn = std::string(), const UnknownElement& elementIn = UnknownElement());

      bool operator == (const Member& member) const;

      std::string name;
      UnknownElement element;
   };

   typedef std::list<Member> Members; // map faster, but does not preserve order
   typedef Members::iterator iterator;
   typedef Members::const_iterator const_iterator;

   bool operator == (const Object& object) const;

   iterator Begin();
   iterator End();
   const_iterator Begin() const;
   const_iterator End() const;

   size_t Size() const;
   bool Empty() const;

   iterator Find(const std::string& name);
   const_iterator Find(const std::string& name) const;
   bool HasKey(const std::string& name) const { const_iterator it=Find(name); return (it != End() ); }

   iterator Insert(const Member& member);
   iterator Insert(const Member& member, iterator itWhere);
   iterator Erase(iterator itWhere);
   void Clear();

   UnknownElement& operator [](const std::string& name);
   const UnknownElement& operator [](const std::string& name) const;
	// Braden's addition: a "GetOrDefault" method that won't raise an exception
	// on non-existent keys, only on invalid conversions.
	const int GetOrDefault(const std::string& name, int defaultVal) const { return (int)ImpGetOrDefault(name, (double)defaultVal); }
	const double GetOrDefault(const std::string& name, double defaultVal) const { return ImpGetOrDefault(name, defaultVal); }
	const std::string GetOrDefault(const std::string& name, const std::string& defaultVal) const { return ImpGetOrDefault(name, defaultVal); }
	const bool GetOrDefault(const std::string& name, bool defaultVal) const { return ImpGetOrDefault(name, defaultVal); }
private:
   template <typename ValueTypeT>
   const ValueTypeT ImpGetOrDefault(const std::string& name, ValueTypeT defaultVal) const;
   class Finder;

   Members m_Members;
};


/////////////////////////////////////////////////////////////////////////////////
// TrivialType_T - class template for encapsulates a simple data type, such as
//  a string, number, or boolean. Provides implicit const & noncost cast operators
//  for that type, allowing "DataTypeT type = trivialType;"


template <typename DataTypeT>
class TrivialType_T
{
public:
   TrivialType_T(const DataTypeT& t = DataTypeT());

   operator DataTypeT&();
   operator const DataTypeT&() const;

   DataTypeT& Value();
   const DataTypeT& Value() const;

   bool operator == (const TrivialType_T<DataTypeT>& trivial) const;

private:
   DataTypeT m_tValue;
};



/////////////////////////////////////////////////////////////////////////////////
// Null - doesn't do much of anything but satisfy the JSON spec. It is the default
//  element type of UnknownElement

class Null
{
public:
   bool operator == (const Null& trivial) const;
};

class Visitor
{
public:
	virtual ~Visitor() {}
	
	virtual void Visit(Array& array) = 0;
	virtual void Visit(Object& object) = 0;
	virtual void Visit(Number& number) = 0;
	virtual void Visit(String& string) = 0;
	virtual void Visit(Boolean& boolean) = 0;
	virtual void Visit(Null& null) = 0;
};

class ConstVisitor
{
public:
	virtual ~ConstVisitor() {}
	
	virtual void Visit(const Array& array) = 0;
	virtual void Visit(const Object& object) = 0;
	virtual void Visit(const Number& number) = 0;
	virtual void Visit(const String& string) = 0;
	virtual void Visit(const Boolean& boolean) = 0;
	virtual void Visit(const Null& null) = 0;
};

inline Exception::Exception(const std::string& sMessage) :
   std::runtime_error(sMessage) {}

/////////////////////////
// UnknownElement members

class UnknownElement::Imp
{
public:
   virtual ~Imp() {}
   virtual Imp* Clone() const = 0;

   virtual bool Compare(const Imp& imp) const = 0;

   virtual void Accept(ConstVisitor& visitor) const = 0;
   virtual void Accept(Visitor& visitor) = 0;
};


template <typename ElementTypeT>
class UnknownElement::Imp_T : public UnknownElement::Imp
{
public:
   Imp_T(const ElementTypeT& element) : m_Element(element) {}
   virtual Imp* Clone() const { return new Imp_T<ElementTypeT>(*this); }

   virtual void Accept(ConstVisitor& visitor) const { visitor.Visit(m_Element); }
   virtual void Accept(Visitor& visitor) { visitor.Visit(m_Element); }

   virtual bool Compare(const Imp& imp) const
   {
      ConstCastVisitor_T<ElementTypeT> castVisitor;
      imp.Accept(castVisitor);
      return castVisitor.m_pElement &&
             m_Element == *castVisitor.m_pElement;
   }

private:
   ElementTypeT m_Element;
};


class UnknownElement::ConstCastVisitor : public ConstVisitor
{
   virtual void Visit(const Array& array) {}
   virtual void Visit(const Object& object) {}
   virtual void Visit(const Number& number) {}
   virtual void Visit(const String& string) {}
   virtual void Visit(const Boolean& boolean) {}
   virtual void Visit(const Null& null) {}
};

template <typename ElementTypeT>
class UnknownElement::ConstCastVisitor_T : public ConstCastVisitor
{
public:
   ConstCastVisitor_T() : m_pElement(0) {}
   virtual void Visit(const ElementTypeT& element) { m_pElement = &element; } // we don't know what this is, but it overrides one of the base's no-op functions
   const ElementTypeT* m_pElement;
};

// Braden's addition: specialize the Boolean template so we can cast numbers to bool
template <>
class UnknownElement::ConstCastVisitor_T<Boolean> : public ConstCastVisitor
{
public:
	ConstCastVisitor_T<Boolean>() : m_pElement(0) {}
	virtual void Visit(const Boolean& element) { m_pElement = &element; } // Normal casting from UnknownElement to Boolean
	virtual void Visit(const Number& element) { double val=element; if (val == 0 || val == 1) { Visit(m_value = Boolean(val==1)); } } // Conversion casting from Number to Boolean
	const Boolean* m_pElement;
	Boolean m_value; // We need this to temporarily store the value if we've done a conversion.
};
// Braden's addition: specialize the Number template so we can cast bool to double
template <>
class UnknownElement::ConstCastVisitor_T<Number> : public ConstCastVisitor
{
public:
	ConstCastVisitor_T<Number>() : m_pElement(0) {}
	virtual void Visit(const Number& element) { m_pElement = &element; } // Normal casting from UnknownElement to Number
	virtual void Visit(const Boolean& element) { Visit(m_value = Number(element.Value())); } // Conversion casting from Boolean to Number
	const Number* m_pElement;
	Number m_value; // We need this to temporarily store the value if we've done a conversion.
};

class UnknownElement::CastVisitor : public Visitor
{
   virtual void Visit(Array& array) {}
   virtual void Visit(Object& object) {}
   virtual void Visit(Number& number) {}
   virtual void Visit(String& string) {}
   virtual void Visit(Boolean& boolean) {}
   virtual void Visit(Null& null) {}
};

template <typename ElementTypeT>
class UnknownElement::CastVisitor_T : public CastVisitor
{
public:
   CastVisitor_T() : m_pElement(0) {}
   virtual void Visit(ElementTypeT& element) { m_pElement = &element; } // we don't know what this is, but it overrides one of the base's no-op functions
   ElementTypeT* m_pElement;
};

inline UnknownElement::UnknownElement() :                               m_pImp( new Imp_T<Null>( Null() ) ) {}
inline UnknownElement::UnknownElement(const UnknownElement& unknown) :  m_pImp( unknown.m_pImp->Clone()) {}
inline UnknownElement::UnknownElement(const Object& object) :           m_pImp( new Imp_T<Object>(object) ) {}
inline UnknownElement::UnknownElement(const Array& array) :             m_pImp( new Imp_T<Array>(array) ) {}
inline UnknownElement::UnknownElement(const Number& number) :           m_pImp( new Imp_T<Number>(number) ) {}
inline UnknownElement::UnknownElement(const Boolean& boolean) :         m_pImp( new Imp_T<Boolean>(boolean) ) {}
inline UnknownElement::UnknownElement(const String& string) :           m_pImp( new Imp_T<String>(string) ) {}
inline UnknownElement::UnknownElement(const Null& null) :               m_pImp( new Imp_T<Null>(null) ) {}

inline UnknownElement::~UnknownElement()   { delete m_pImp; }

inline UnknownElement::operator const Object& () const    { return CastTo<Object>(); }
inline UnknownElement::operator const Array& () const     { return CastTo<Array>(); }
inline UnknownElement::operator const Number& () const    { return CastTo<Number>(); }
inline UnknownElement::operator const Boolean& () const   { return CastTo<Boolean>(); }
inline UnknownElement::operator const String& () const    { return CastTo<String>(); }
inline UnknownElement::operator const Null& () const      { return CastTo<Null>(); }

inline UnknownElement::operator Object& ()    { return ConvertTo<Object>(); }
inline UnknownElement::operator Array& ()     { return ConvertTo<Array>(); }
inline UnknownElement::operator Number& ()    { return ConvertTo<Number>(); }
inline UnknownElement::operator Boolean& ()   { return ConvertTo<Boolean>(); }
inline UnknownElement::operator String& ()    { return ConvertTo<String>(); }
inline UnknownElement::operator Null& ()      { return ConvertTo<Null>(); }

inline UnknownElement& UnknownElement::operator = (const UnknownElement& unknown) 
{
   // always check for this
   if (&unknown != this)
   {
      // we might be copying from a subtree of ourselves. delete the old imp
      //  only after the clone operation is complete. yes, this could be made 
      //  more efficient, but isn't worth the complexity
      Imp* pOldImp = m_pImp;
      m_pImp = unknown.m_pImp->Clone();
      delete pOldImp;
   }

   return *this;
}

inline UnknownElement& UnknownElement::operator[] (const std::string& key)
{
   // the people want an object. make us one if we aren't already
   Object& object = ConvertTo<Object>();
   return object[key];
}

inline const UnknownElement& UnknownElement::operator[] (const std::string& key) const
{
   // throws if we aren't an object
   const Object& object = CastTo<Object>();
   return object[key];
}

inline UnknownElement& UnknownElement::operator[] (size_t index)
{
   // the people want an array. make us one if we aren't already
   Array& array = ConvertTo<Array>();
   return array[index];
}

inline const UnknownElement& UnknownElement::operator[] (size_t index) const
{
   // throws if we aren't an array
   const Array& array = CastTo<Array>();
   return array[index];
}


template <typename ElementTypeT>
const ElementTypeT& UnknownElement::CastTo() const
{
   ConstCastVisitor_T<ElementTypeT> castVisitor;
   m_pImp->Accept(castVisitor);
   if (castVisitor.m_pElement == 0)
      throw Exception("Bad cast");
   return *castVisitor.m_pElement;
}

template <typename ElementTypeT>
bool UnknownElement::IsOfType() const
{
   ConstCastVisitor_T<ElementTypeT> castVisitor;
   m_pImp->Accept(castVisitor);
   return (castVisitor.m_pElement != 0);
}




template <typename ElementTypeT>
ElementTypeT& UnknownElement::ConvertTo() 
{
   CastVisitor_T<ElementTypeT> castVisitor;
   m_pImp->Accept(castVisitor);
   if (castVisitor.m_pElement == 0)
   {
      // we're not the right type. fix it & try again
      *this = ElementTypeT();
      m_pImp->Accept(castVisitor);
   }

   return *castVisitor.m_pElement;
}


inline void UnknownElement::Accept(ConstVisitor& visitor) const { m_pImp->Accept(visitor); }
inline void UnknownElement::Accept(Visitor& visitor)            { m_pImp->Accept(visitor); }


inline bool UnknownElement::operator == (const UnknownElement& element) const
{
   return m_pImp->Compare(*element.m_pImp);
}



//////////////////
// Object members


inline Object::Member::Member(const std::string& nameIn, const UnknownElement& elementIn) :
   name(nameIn), element(elementIn) {}

inline bool Object::Member::operator == (const Member& member) const 
{
   return name == member.name &&
          element == member.element;
}

class Object::Finder : public std::unary_function<Object::Member, bool>
{
public:
   Finder(const std::string& name) : m_name(name) {}
   bool operator () (const Object::Member& member) {
      return member.name == m_name;
   }

private:
   std::string m_name;
};



inline Object::iterator Object::Begin() { return m_Members.begin(); }
inline Object::iterator Object::End() { return m_Members.end(); }
inline Object::const_iterator Object::Begin() const { return m_Members.begin(); }
inline Object::const_iterator Object::End() const { return m_Members.end(); }

inline size_t Object::Size() const { return m_Members.size(); }
inline bool Object::Empty() const { return m_Members.empty(); }

inline Object::iterator Object::Find(const std::string& name) 
{
   return std::find_if(m_Members.begin(), m_Members.end(), Finder(name));
}

inline Object::const_iterator Object::Find(const std::string& name) const 
{
   return std::find_if(m_Members.begin(), m_Members.end(), Finder(name));
}

inline Object::iterator Object::Insert(const Member& member)
{
   return Insert(member, End());
}

inline Object::iterator Object::Insert(const Member& member, iterator itWhere)
{
   iterator it = Find(member.name);
   if (it != m_Members.end())
      throw Exception(std::string("Object member already exists: ") + member.name);

   it = m_Members.insert(itWhere, member);
   return it;
}

inline Object::iterator Object::Erase(iterator itWhere) 
{
   return m_Members.erase(itWhere);
}

inline UnknownElement& Object::operator [](const std::string& name)
{

   iterator it = Find(name);
   if (it == m_Members.end())
   {
      Member member(name);
      it = Insert(member, End());
   }
   return it->element;      
}

inline const UnknownElement& Object::operator [](const std::string& name) const 
{
   const_iterator it = Find(name);
   if (it == End())
      throw Exception(std::string("Object member not found: ") + name);
   return it->element;
}

// Addition by Braden - Implementation of GetOrDefault
template <typename ValueTypeT>
const ValueTypeT Object::ImpGetOrDefault(const std::string& name, ValueTypeT defaultVal) const
{
	const_iterator it = Find(name);
	if (it == End())
		return defaultVal;
	return (TrivialType_T<ValueTypeT>)it->element;
}

inline void Object::Clear() 
{
   m_Members.clear(); 
}

inline bool Object::operator == (const Object& object) const 
{
   return m_Members == object.m_Members;
}


/////////////////
// Array members

inline Array::iterator Array::Begin()  { return m_Elements.begin(); }
inline Array::iterator Array::End()    { return m_Elements.end(); }
inline Array::const_iterator Array::Begin() const  { return m_Elements.begin(); }
inline Array::const_iterator Array::End() const    { return m_Elements.end(); }

inline Array::iterator Array::Insert(const UnknownElement& element, iterator itWhere)
{ 
   return m_Elements.insert(itWhere, element);
}

inline Array::iterator Array::Insert(const UnknownElement& element)
{
   return Insert(element, End());
}

inline Array::iterator Array::Erase(iterator itWhere)
{ 
   return m_Elements.erase(itWhere);
}

inline void Array::Resize(size_t newSize)
{
   m_Elements.resize(newSize);
}

inline size_t Array::Size() const  { return m_Elements.size(); }
inline bool Array::Empty() const   { return m_Elements.empty(); }

inline UnknownElement& Array::operator[] (size_t index)
{
   size_t nMinSize = index + 1; // zero indexed
   if (m_Elements.size() < nMinSize)
      m_Elements.resize(nMinSize);
   return m_Elements[index]; 
}

inline const UnknownElement& Array::operator[] (size_t index) const 
{
   if (index >= m_Elements.size())
      throw Exception("Array out of bounds");
   return m_Elements[index]; 
}

inline void Array::Clear() {
   m_Elements.clear();
}

inline bool Array::operator == (const Array& array) const
{
   return m_Elements == array.m_Elements;
}


////////////////////////
// TrivialType_T members

template <typename DataTypeT>
TrivialType_T<DataTypeT>::TrivialType_T(const DataTypeT& t) :
   m_tValue(t) {}

template <typename DataTypeT>
TrivialType_T<DataTypeT>::operator DataTypeT&()
{
   return Value(); 
}

template <typename DataTypeT>
TrivialType_T<DataTypeT>::operator const DataTypeT&() const
{
   return Value(); 
}

template <typename DataTypeT>
DataTypeT& TrivialType_T<DataTypeT>::Value()
{
   return m_tValue; 
}

template <typename DataTypeT>
const DataTypeT& TrivialType_T<DataTypeT>::Value() const
{
   return m_tValue; 
}

template <typename DataTypeT>
bool TrivialType_T<DataTypeT>::operator == (const TrivialType_T<DataTypeT>& trivial) const
{
   return m_tValue == trivial.m_tValue;
}



//////////////////
// Null members

inline bool Null::operator == (const Null& trivial) const
{
   return true;
}


class Reader
{
public:
   // this structure will be reported in one of the exceptions defined below
   struct Location
   {
      Location();

      unsigned int m_nLine;       // document line, zero-indexed
      unsigned int m_nLineOffset; // character offset from beginning of line, zero indexed
      unsigned int m_nDocOffset;  // character offset from entire document, zero indexed
   };

   // thrown during the first phase of reading. generally catches low-level problems such
   //  as errant characters or corrupt/incomplete documents
   class ScanException : public Exception
   {
   public:
      ScanException(const std::string& sMessage, const Reader::Location& locError) :
         Exception(sMessage),
         m_locError(locError) {}

      Reader::Location m_locError;
   };

   // thrown during the second phase of reading. generally catches higher-level problems such
   //  as missing commas or brackets
   class ParseException : public Exception
   {
   public:
      ParseException(const std::string& sMessage, const Reader::Location& locTokenBegin, const Reader::Location& locTokenEnd) :
         Exception(sMessage),
         m_locTokenBegin(locTokenBegin),
         m_locTokenEnd(locTokenEnd) {}

      Reader::Location m_locTokenBegin;
      Reader::Location m_locTokenEnd;
   };


   // if you know what the document looks like, call one of these...
   static void Read(Object& object, std::istream& istr);
   static void Read(Array& array, std::istream& istr);
   static void Read(String& string, std::istream& istr);
   static void Read(Number& number, std::istream& istr);
   static void Read(Boolean& boolean, std::istream& istr);
   static void Read(Null& null, std::istream& istr);

   // ...otherwise, if you don't know, call this & visit it
   static void Read(UnknownElement& elementRoot, std::istream& istr);

private:
   struct Token
   {
      enum Type
      {
         TOKEN_OBJECT_BEGIN,  //    {
         TOKEN_OBJECT_END,    //    }
         TOKEN_ARRAY_BEGIN,   //    [
         TOKEN_ARRAY_END,     //    ]
         TOKEN_NEXT_ELEMENT,  //    ,
         TOKEN_MEMBER_ASSIGN, //    :
         TOKEN_STRING,        //    "xxx"
         TOKEN_NUMBER,        //    [+/-]000.000[e[+/-]000]
         TOKEN_BOOLEAN,       //    true -or- false
         TOKEN_NULL,          //    null
      };

      Token() : nType(TOKEN_NULL), sValue(NULL) {}
      Token(const Token& t) : nType(t.nType), sValue(NULL) { if (t.sValue) SetStrValue(t.GetStrValue()); }
      Token& operator=(const Token& r) { if (&r != this) { nType = r.nType; if (r.sValue) SetStrValue(r.sValue); else sValue = NULL; } return *this; }
      ~Token() { if (sValue && sValue != sValueBuffer) delete[] sValue; }

      Type nType;

      void SetStrValue(const char* val) {
         assert(sValue == NULL); // Should only be set once
         unsigned int length_needed = strlen(val)+1;
		 if (length_needed > sizeof(sValueBuffer)/sizeof(char)) {
            sValue = new char[length_needed];
            memcpy(sValue, val, length_needed);
         } else {
            memcpy(sValueBuffer, val, length_needed);
            sValue = sValueBuffer;
         }
      }
      void SetStrValue(const char val) {
         assert(sValue == NULL);
         sValueBuffer[0] = val;
         sValueBuffer[1] = '\0';
         sValue = sValueBuffer;
      }
      const char* GetStrValue() const {
         assert(sValue != NULL);
         return sValue;
      }

      // for malformed file debugging
      Reader::Location locBegin;
      Reader::Location locEnd;
   private:
      char sValueBuffer[8]; // Optimization: for short string values, store the string in this buffer, rather than triggering a malloc()
      char* sValue;
   };

   class InputStream;
   class TokenStream;
   typedef std::vector<Token> Tokens;

   template <typename ElementTypeT>   
   static void Read_i(ElementTypeT& element, std::istream& istr);

   // scanning istream into token sequence
   void Scan(Tokens& tokens, InputStream& inputStream);

   void EatWhiteSpace(InputStream& inputStream);
   std::string MatchString(InputStream& inputStream);
   std::string MatchNumber(InputStream& inputStream);
   const char* MatchExpectedString(InputStream& inputStream, const char* sExpected);

   // parsing token sequence into element structure
   void Parse(UnknownElement& element, TokenStream& tokenStream);
   void Parse(Object& object, TokenStream& tokenStream);
   void Parse(Array& array, TokenStream& tokenStream);
   void Parse(String& string, TokenStream& tokenStream);
   void Parse(Number& number, TokenStream& tokenStream);
   void Parse(Boolean& boolean, TokenStream& tokenStream);
   void Parse(Null& null, TokenStream& tokenStream);

   const char* MatchExpectedToken(Token::Type nExpected, TokenStream& tokenStream);
};


inline std::istream& operator >> (std::istream& istr, UnknownElement& elementRoot) {
   Reader::Read(elementRoot, istr);
   return istr;
}

inline Reader::Location::Location() :
   m_nLine(0),
   m_nLineOffset(0),
   m_nDocOffset(0)
{}


//////////////////////
// Reader::InputStream

class Reader::InputStream // would be cool if we could inherit from std::istream & override "get"
{
public:
   InputStream(std::istream& iStr) :
      m_iStr(iStr) {}

   // protect access to the input stream, so we can keeep track of document/line offsets
   char Get(); // big, define outside
   char Peek() {
      assert(m_iStr.eof() == false); // enforce reading of only valid stream data 
      return m_iStr.peek();
   }

   bool EOS() {
      m_iStr.peek(); // apparently eof flag isn't set until a character read is attempted. whatever.
      return m_iStr.eof();
   }

   const Location& GetLocation() const { return m_Location; }

private:
   std::istream& m_iStr;
   Location m_Location;
};


inline char Reader::InputStream::Get()
{
   assert(m_iStr.eof() == false); // enforce reading of only valid stream data 
   char c = m_iStr.get();
   
   ++m_Location.m_nDocOffset;
   if (c == '\n') {
      ++m_Location.m_nLine;
      m_Location.m_nLineOffset = 0;
   }
   else {
      ++m_Location.m_nLineOffset;
   }

   return c;
}



//////////////////////
// Reader::TokenStream

class Reader::TokenStream
{
public:
   TokenStream(const Tokens& tokens);

   const Token& Peek();
   const Token& Get();

   bool EOS() const;

private:
   const Tokens& m_Tokens;
   Tokens::const_iterator m_itCurrent;
};


inline Reader::TokenStream::TokenStream(const Tokens& tokens) :
   m_Tokens(tokens),
   m_itCurrent(tokens.begin())
{}

inline const Reader::Token& Reader::TokenStream::Peek() {
   if (EOS())
   {
      const Token& lastToken = *m_Tokens.rbegin();
      std::string sMessage = "Unexpected end of token stream";
      throw ParseException(sMessage, lastToken.locBegin, lastToken.locEnd); // nowhere to point to
   }
   return *(m_itCurrent); 
}

inline const Reader::Token& Reader::TokenStream::Get() {
   const Token& token = Peek();
   ++m_itCurrent;
   return token;
}

inline bool Reader::TokenStream::EOS() const {
   return m_itCurrent == m_Tokens.end(); 
}

///////////////////
// Reader (finally)


inline void Reader::Read(Object& object, std::istream& istr)                { Read_i(object, istr); }
inline void Reader::Read(Array& array, std::istream& istr)                  { Read_i(array, istr); }
inline void Reader::Read(String& string, std::istream& istr)                { Read_i(string, istr); }
inline void Reader::Read(Number& number, std::istream& istr)                { Read_i(number, istr); }
inline void Reader::Read(Boolean& boolean, std::istream& istr)              { Read_i(boolean, istr); }
inline void Reader::Read(Null& null, std::istream& istr)                    { Read_i(null, istr); }
inline void Reader::Read(UnknownElement& unknown, std::istream& istr)       { Read_i(unknown, istr); }


template <typename ElementTypeT>   
void Reader::Read_i(ElementTypeT& element, std::istream& istr)
{
   Reader reader;

   Tokens tokens;
   InputStream inputStream(istr);
   reader.Scan(tokens, inputStream);

   TokenStream tokenStream(tokens);
   reader.Parse(element, tokenStream);

   if (tokenStream.EOS() == false)
   {
      const Token& token = tokenStream.Peek();
      std::string sMessage = std::string("Expected End of token stream; found ") + token.GetStrValue();
      throw ParseException(sMessage, token.locBegin, token.locEnd);
   }
}


inline void Reader::Scan(Tokens& tokens, InputStream& inputStream)
{
   while (EatWhiteSpace(inputStream),              // ignore any leading white space...
          inputStream.EOS() == false) // ...before checking for EOS
   {
      // if all goes well, we'll create a token each pass
      Token token;
      token.locBegin = inputStream.GetLocation();

      // gives us null-terminated string
      char sChar = inputStream.Peek();
      switch (sChar)
      {
         case '{':
            token.SetStrValue(inputStream.Get());
            token.nType = Token::TOKEN_OBJECT_BEGIN;
            break;

         case '}':
            token.SetStrValue(inputStream.Get());
            token.nType = Token::TOKEN_OBJECT_END;
            break;

         case '[':
            token.SetStrValue(inputStream.Get());
            token.nType = Token::TOKEN_ARRAY_BEGIN;
            break;

         case ']':
            token.SetStrValue(inputStream.Get());
            token.nType = Token::TOKEN_ARRAY_END;
            break;

         case ',':
            token.SetStrValue(inputStream.Get());
            token.nType = Token::TOKEN_NEXT_ELEMENT;
            break;

         case ':':
            token.SetStrValue(inputStream.Get());
            token.nType = Token::TOKEN_MEMBER_ASSIGN;
            break;

         case '"':
            token.SetStrValue(MatchString(inputStream).c_str());
            token.nType = Token::TOKEN_STRING;
            break;

         case '-':
         case '0':
         case '1':
         case '2':
         case '3':
         case '4':
         case '5':
         case '6':
         case '7':
         case '8':
         case '9':
            token.SetStrValue(MatchNumber(inputStream).c_str());
            token.nType = Token::TOKEN_NUMBER;
            break;

         case 't':
            token.SetStrValue(MatchExpectedString(inputStream, "true"));
            token.nType = Token::TOKEN_BOOLEAN;
            break;

         case 'f':
            token.SetStrValue(MatchExpectedString(inputStream, "false"));
            token.nType = Token::TOKEN_BOOLEAN;
            break;

         case 'n':
            ;
            token.SetStrValue(MatchExpectedString(inputStream, "null"));
            token.nType = Token::TOKEN_NULL;
            break;

         default:
         {
            std::string sErrorMessage = std::string("Unexpected character in stream: ") + sChar;
            throw ScanException(sErrorMessage, inputStream.GetLocation());
         }
      }

      token.locEnd = inputStream.GetLocation();
      tokens.push_back(token);
   }
}


inline void Reader::EatWhiteSpace(InputStream& inputStream)
{
   while (inputStream.EOS() == false && 
          ::isspace(inputStream.Peek()))
      inputStream.Get();
}

inline const char* Reader::MatchExpectedString(InputStream& inputStream, const char* sExpected)
{
   for (const char* it = sExpected; *it != '\0'; ++it) {
      if (inputStream.EOS() ||      // did we reach the end before finding what we're looking for...
          inputStream.Get() != *it) // ...or did we find something different?
      {
         std::string sMessage = std::string("Expected string: ") + sExpected;
         throw ScanException(sMessage, inputStream.GetLocation());
      }
   }
   // all's well if we made it here
   return sExpected;
}


inline std::string Reader::MatchString(InputStream& inputStream)
{
   //MatchExpectedString(inputStream, "\"");
   if (inputStream.EOS() || inputStream.Get() != '"')
   {
      throw ScanException("Expected quotation mark: \"", inputStream.GetLocation());
   }

   std::string string;
   while (inputStream.EOS() == false)
   {
      char c = inputStream.Get();

      // escape?
      if (c == '\\' &&
          inputStream.EOS() == false) // shouldn't have reached the end yet
      {
         c = inputStream.Get();
         switch (c) {
            case '/':      string.push_back('/');     break;
            case '"':      string.push_back('"');     break;
            case '\\':     string.push_back('\\');    break;
            case 'b':      string.push_back('\b');    break;
            case 'f':      string.push_back('\f');    break;
            case 'n':      string.push_back('\n');    break;
            case 'r':      string.push_back('\r');    break;
            case 't':      string.push_back('\t');    break;
            case 'u':
			   // Unicode parsing:
			   char hex[5];
			   int val;
			   for (int i=0;i<4 && !inputStream.EOS();i++)
				  hex[i] = inputStream.Get();
			   hex[4] = '\0';
			   if (sscanf(hex, "%4x", &val) != 1)
			      throw ScanException("Unable to parse unicode escape", inputStream.GetLocation());
			   // Convert from UTF-16 to UTF-8:
			   if (0xD800 <= val && val <= 0xDBFF)
			      throw ScanException("Unicode escape sequence is outside of supported range.", inputStream.GetLocation());
			   if (val <= 0x7F) { string.push_back((char)val); }
			   else if (val <= 0x7FF) { string.push_back(char(0xC0 | ((val>>6) & 0x1F))); string.push_back(char(0x80 | (val & 0x3F))); }
			   else if (val <= 0xFFFF) { string.push_back(char(0xE0 | ((val>>12) & 0x0F))); string.push_back(char(0x80 | ((val>>6) & 0x3F))); string.push_back(char(0x80 | (val & 0x3F))); }
			   break;
            default: {
               std::string sMessage = std::string("Unrecognized escape sequence found in string: \\") + c;
               throw ScanException(sMessage, inputStream.GetLocation());
            }
         }
      } else if (c == '"') {
         // We have reached the closing quote
         return string;
      } else {
         string.push_back(c);
      }
   }

   // We should not get here.
   throw ScanException("Expected quotation mark \" before end of stream.", inputStream.GetLocation());
}


inline std::string Reader::MatchNumber(InputStream& inputStream)
{
   static const char sNumericChars[] = "0123456789.eE-+";
   static const char* end = sNumericChars + sizeof(sNumericChars)/sizeof(char);

   std::string sNumber;
   while (inputStream.EOS() == false &&
          std::find(sNumericChars, end, inputStream.Peek()) != end)
   {
      sNumber.push_back(inputStream.Get());
   }

   return sNumber;
}


inline void Reader::Parse(UnknownElement& element, Reader::TokenStream& tokenStream) 
{
   const Token& token = tokenStream.Peek();
   switch (token.nType) {
      case Token::TOKEN_OBJECT_BEGIN:
      {
         // implicit non-const cast will perform conversion for us (if necessary)
         Object& object = element;
         Parse(object, tokenStream);
         break;
      }

      case Token::TOKEN_ARRAY_BEGIN:
      {
         Array& array = element;
         Parse(array, tokenStream);
         break;
      }

      case Token::TOKEN_STRING:
      {
         String& string = element;
         Parse(string, tokenStream);
         break;
      }

      case Token::TOKEN_NUMBER:
      {
         Number& number = element;
         Parse(number, tokenStream);
         break;
      }

      case Token::TOKEN_BOOLEAN:
      {
         Boolean& boolean = element;
         Parse(boolean, tokenStream);
         break;
      }

      case Token::TOKEN_NULL:
      {
         Null& null = element;
         Parse(null, tokenStream);
         break;
      }

      default:
      {
         std::string sMessage = std::string("Unexpected token: ") + token.GetStrValue();
         throw ParseException(sMessage, token.locBegin, token.locEnd);
      }
   }
}


inline void Reader::Parse(Object& object, Reader::TokenStream& tokenStream)
{
   MatchExpectedToken(Token::TOKEN_OBJECT_BEGIN, tokenStream);

   bool bContinue = (tokenStream.EOS() == false &&
                     tokenStream.Peek().nType != Token::TOKEN_OBJECT_END);
   while (bContinue)
   {
      Object::Member member;

      // first the member name. save the token in case we have to throw an exception
      const Token& tokenName = tokenStream.Peek();
      member.name = MatchExpectedToken(Token::TOKEN_STRING, tokenStream);

      // ...then the key/value separator...
      MatchExpectedToken(Token::TOKEN_MEMBER_ASSIGN, tokenStream);

      // ...then the value itself (can be anything).
      Parse(member.element, tokenStream);

      // try adding it to the object (this could throw)
      try
      {
         object.Insert(member);
      }
      catch (Exception&)
      {
         // must be a duplicate name
         std::string sMessage = std::string("Duplicate object member token: ") + member.name; 
         throw ParseException(sMessage, tokenName.locBegin, tokenName.locEnd);
      }

      bContinue = (tokenStream.EOS() == false &&
                   tokenStream.Peek().nType == Token::TOKEN_NEXT_ELEMENT);
      if (bContinue)
         MatchExpectedToken(Token::TOKEN_NEXT_ELEMENT, tokenStream);
   }

   MatchExpectedToken(Token::TOKEN_OBJECT_END, tokenStream);
}


inline void Reader::Parse(Array& array, Reader::TokenStream& tokenStream)
{
   MatchExpectedToken(Token::TOKEN_ARRAY_BEGIN, tokenStream);

   bool bContinue = (tokenStream.EOS() == false &&
                     tokenStream.Peek().nType != Token::TOKEN_ARRAY_END);
   while (bContinue)
   {
      // ...what's next? could be anything
      Array::iterator itElement = array.Insert(UnknownElement());
      UnknownElement& element = *itElement;
      Parse(element, tokenStream);

      bContinue = (tokenStream.EOS() == false &&
                   tokenStream.Peek().nType == Token::TOKEN_NEXT_ELEMENT);
      if (bContinue)
         MatchExpectedToken(Token::TOKEN_NEXT_ELEMENT, tokenStream);
   }

   MatchExpectedToken(Token::TOKEN_ARRAY_END, tokenStream);
}


inline void Reader::Parse(String& string, Reader::TokenStream& tokenStream)
{
   string = std::string(MatchExpectedToken(Token::TOKEN_STRING, tokenStream));
}


inline void Reader::Parse(Number& number, Reader::TokenStream& tokenStream)
{
   const Token& currentToken = tokenStream.Peek(); // might need this later for throwing exception
   const char* sValue = MatchExpectedToken(Token::TOKEN_NUMBER, tokenStream);

   // istringstream double parsing is terribly slow, so try to parse the number ourselves:
   int sign = 1;
   double value = 0;
   if (*sValue == '-') {
      sign = -1;
      sValue++;
   }
   while (*sValue >= '0' && *sValue <= '9')
      value = value*10 + (*sValue++ - '0');
   if (*sValue == '.') {
      // Parse the part after the decimal point:
      long double decimal = 0;
      long double order = 0.1;
      sValue++;
      while (*sValue >= '0' && *sValue <= '9') {
         decimal += (*sValue++ - '0')*order;
         order *= 0.1;
      }
      if (*sValue == '\0') {
         // We parsed the whole number.
         number = sign * (value + decimal);
         return;
      }
   } else if (*sValue == '\0') {
      // We parsed the whole number.
      number = sign * value;
      return;
   }

   // If we got here, the number is likely in scientific notation.
   // Let's just let istringstream handle it:
   std::istringstream iStr(sValue);
   double dValue;
   iStr >> dValue;

   // did we consume all characters in the token?
   if (iStr.eof() == false)
   {
      char c = iStr.peek();
      std::string sMessage = std::string("Unexpected character in NUMBER token: ") + c;
      throw ParseException(sMessage, currentToken.locBegin, currentToken.locEnd);
   }

   number = dValue;
}


inline void Reader::Parse(Boolean& boolean, Reader::TokenStream& tokenStream)
{
   const std::string sValue = MatchExpectedToken(Token::TOKEN_BOOLEAN, tokenStream);
   boolean = (sValue == "true" ? true : false);
}


inline void Reader::Parse(Null&, Reader::TokenStream& tokenStream)
{
   MatchExpectedToken(Token::TOKEN_NULL, tokenStream);
}


inline const char* Reader::MatchExpectedToken(Token::Type nExpected, Reader::TokenStream& tokenStream)
{
   const Token& token = tokenStream.Get();
   if (token.nType != nExpected)
   {
      std::string sMessage = std::string("Unexpected token: ") + token.GetStrValue();
      throw ParseException(sMessage, token.locBegin, token.locEnd);
   }

   return token.GetStrValue();
}

class Writer : private ConstVisitor
{
public:
   static void Write(const Object& object, std::ostream& ostr);
   static void Write(const Array& array, std::ostream& ostr);
   static void Write(const String& string, std::ostream& ostr);
   static void Write(const Number& number, std::ostream& ostr);
   static void Write(const Boolean& boolean, std::ostream& ostr);
   static void Write(const Null& null, std::ostream& ostr);
   static void Write(const UnknownElement& elementRoot, std::ostream& ostr);

private:
   Writer(std::ostream& ostr);

   template <typename ElementTypeT>
   static void Write_i(const ElementTypeT& element, std::ostream& ostr);

   void Write_i(const Object& object);
   void Write_i(const Array& array);
   void Write_i(const String& string);
   void Write_i(const Number& number);
   void Write_i(const Boolean& boolean);
   void Write_i(const Null& null);
   void Write_i(const UnknownElement& unknown);

   virtual void Visit(const Array& array);
   virtual void Visit(const Object& object);
   virtual void Visit(const Number& number);
   virtual void Visit(const String& string);
   virtual void Visit(const Boolean& boolean);
   virtual void Visit(const Null& null);

   std::ostream& m_ostr;
   int m_nTabDepth;
};

inline void Writer::Write(const UnknownElement& elementRoot, std::ostream& ostr) { Write_i(elementRoot, ostr); }
inline void Writer::Write(const Object& object, std::ostream& ostr)              { Write_i(object, ostr); }
inline void Writer::Write(const Array& array, std::ostream& ostr)                { Write_i(array, ostr); }
inline void Writer::Write(const Number& number, std::ostream& ostr)              { Write_i(number, ostr); }
inline void Writer::Write(const String& string, std::ostream& ostr)              { Write_i(string, ostr); }
inline void Writer::Write(const Boolean& boolean, std::ostream& ostr)            { Write_i(boolean, ostr); }
inline void Writer::Write(const Null& null, std::ostream& ostr)                  { Write_i(null, ostr); }


inline Writer::Writer(std::ostream& ostr) :
   m_ostr(ostr),
   m_nTabDepth(0)
{}

template <typename ElementTypeT>
void Writer::Write_i(const ElementTypeT& element, std::ostream& ostr)
{
   Writer writer(ostr);
   writer.Write_i(element);
   ostr.flush(); // all done
}

inline void Writer::Write_i(const Array& array)
{
   if (array.Empty())
      m_ostr << "[]";
   else
   {
      m_ostr << '[' << std::endl;
      ++m_nTabDepth;

      Array::const_iterator it(array.Begin()),
                            itEnd(array.End());
      while (it != itEnd) {
         m_ostr << std::string(m_nTabDepth, '\t');
         
         Write_i(*it);

         if (++it != itEnd)
            m_ostr << ',';
         m_ostr << std::endl;
      }

      --m_nTabDepth;
      m_ostr << std::string(m_nTabDepth, '\t') << ']';
   }
}

inline void Writer::Write_i(const Object& object)
{
   if (object.Empty())
      m_ostr << "{}";
   else
   {
      m_ostr << '{' << std::endl;
      ++m_nTabDepth;

      Object::const_iterator it(object.Begin()),
                             itEnd(object.End());
      while (it != itEnd) {
         m_ostr << std::string(m_nTabDepth, '\t');
         
         Write_i(it->name);

         m_ostr << " : ";
         Write_i(it->element); 

         if (++it != itEnd)
            m_ostr << ',';
         m_ostr << std::endl;
      }

      --m_nTabDepth;
      m_ostr << std::string(m_nTabDepth, '\t') << '}';
   }
}

inline void Writer::Write_i(const Number& numberElement)
{
   m_ostr << std::setprecision(20) << numberElement.Value();
}

inline void Writer::Write_i(const Boolean& booleanElement)
{
   m_ostr << (booleanElement.Value() ? "true" : "false");
}

inline void Writer::Write_i(const String& stringElement)
{
   m_ostr << '"';

   const std::string& s = stringElement.Value();
   std::string::const_iterator it(s.begin()),
                               itEnd(s.end());
   for (; it != itEnd; ++it)
   {
      switch (*it)
      {
         case '"':         m_ostr << "\\\"";   break;
         case '\\':        m_ostr << "\\\\";   break;
         case '\b':        m_ostr << "\\b";    break;
         case '\f':        m_ostr << "\\f";    break;
         case '\n':        m_ostr << "\\n";    break;
         case '\r':        m_ostr << "\\r";    break;
         case '\t':        m_ostr << "\\t";    break;
         //case '\u':        m_ostr << "\\u";    break; // uh...
         default:          m_ostr << *it;      break;
      }
   }

   m_ostr << '"';   
}

inline void Writer::Write_i(const Null& )
{
   m_ostr << "null";
}

inline void Writer::Write_i(const UnknownElement& unknown)
{
   unknown.Accept(*this); 
}

inline void Writer::Visit(const Array& array)       { Write_i(array); }
inline void Writer::Visit(const Object& object)     { Write_i(object); }
inline void Writer::Visit(const Number& number)     { Write_i(number); }
inline void Writer::Visit(const String& string)     { Write_i(string); }
inline void Writer::Visit(const Boolean& boolean)   { Write_i(boolean); }
inline void Writer::Visit(const Null& null)         { Write_i(null); }



} // End namespace
