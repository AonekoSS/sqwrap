//===========================================================================
//
//  sqwrap - simple Squirrel binder with function overload.
//
//===========================================================================

#include <limits>
#include <string>
#include <sstream>
#include <exception>
#include <mem.h>

// チェックの有無を切り替え
#ifndef SQWRAP_NOCHECK
	#define SQ_SETPARAMSCHECK(v,n,p) sq_setparamscheck(v,n,p)
	#define SQ_SETNATIVECLOSURENAME(v,i,n) sq_setnativeclosurename(v,i,n)
#else
	#define SQ_SETPARAMSCHECK(vm,n,p)
	#define SQ_SETNATIVECLOSURENAME(v,i,p)
#endif

// 引数テンプレ
#define __C1__ class A1
#define __C2__ class A1, class A2
#define __C3__ class A1, class A2, class A3
#define __C4__ class A1, class A2, class A3, class A4
#define __C5__ class A1, class A2, class A3, class A4, class A5
#define __C6__ class A1, class A2, class A3, class A4, class A5, class A6
#define __C7__ class A1, class A2, class A3, class A4, class A5, class A6, class A7
#define __C8__ class A1, class A2, class A3, class A4, class A5, class A6, class A7, class A8
#define __A1__ A1
#define __A2__ A1,A2
#define __A3__ A1,A2,A3
#define __A4__ A1,A2,A3,A4
#define __A5__ A1,A2,A3,A4,A5
#define __A6__ A1,A2,A3,A4,A5,A6
#define __A7__ A1,A2,A3,A4,A5,A6,A7
#define __A8__ A1,A2,A3,A4,A5,A6,A7,A8

// 例外キャッチ
#define __CATCH__ catch(std::exception& e){ return sq_throwerror(v, e.what()); }

namespace sqwrap{
namespace implements{
	// 型の略称
	typedef SQObjectType type_t;
	typedef SQInteger int_t;
	typedef SQFloat real_t;
	typedef SQBool bool_t;
	typedef SQChar char_t;
	typedef SQUserPointer ptr_t;
	typedef const SQChar* cstr_t;
	typedef std::basic_string<SQChar> str_t;
	typedef std::basic_stringstream<SQChar> stream_t;

	// オブジェクト生成
	int_t newtable(HSQUIRRELVM vm){ sq_newtable(vm); return sq_gettop(vm); }
	int_t newclass(HSQUIRRELVM vm){ sq_newclass(vm,false); return sq_gettop(vm); }
	int_t newarray(HSQUIRRELVM vm, int_t size){ sq_newarray(vm,size); return sq_gettop(vm); }

	// パラメータエラー
	std::runtime_error param_error(int index, cstr_t type){
		stream_t s;
		s << "parameter " << (index-1) << " is not " << type;
		return std::runtime_error(s.str());
	}

	// スロット登録
	void bind(HSQUIRRELVM vm, int_t base, cstr_t name, int_t index){
		sq_pushstring(vm, name, -1);
		sq_push(vm, index);
		sq_newslot(vm, base, false);
	}

	// スタック保持
	struct section_t{
		const HSQUIRRELVM vm_;
		const int_t base_;
		section_t(HSQUIRRELVM vm):vm_(vm), base_(sq_gettop(vm)){}
		~section_t(){ sq_settop(vm_,base_); }
	};


//---------------------------------------------------------------------------
//	型特性
//---------------------------------------------------------------------------
	template <class T,
		bool N=std::numeric_limits<T>::is_specialized,
		bool I=std::numeric_limits<T>::is_integer>
	struct X{};

	// INTEGER
	template <class T>
	struct X<T,true,true>{
		static const char_t m = 'n';
		static const char_t t = m;
		static void put(HSQUIRRELVM v, T val){ sq_pushinteger(v,val); }
		static T get(HSQUIRRELVM v, int_t i){
			int_t  a; if(SQ_SUCCEEDED(sq_getinteger(v,i,&a)))return static_cast<T>(a);
			bool_t b; if(SQ_SUCCEEDED(sq_getbool(v,i,&b)))return static_cast<T>(b);
			throw param_error(i,"integer");
		}
	};
	// FLOAT
	template <class T>
	struct X<T,true,false>{
		static const char_t m = 'n';
		static const char_t t = m;
		static str_t s(){ return _SC("n"); }
		static void put(HSQUIRRELVM v, T val){ sq_pushfloat(v,val); }
		static T get(HSQUIRRELVM v, int_t i){
			real_t a; if(SQ_SUCCEEDED(sq_getfloat(v,i,&a)))return static_cast<T>(a);
			int_t  b; if(SQ_SUCCEEDED(sq_getinteger(v,i,&b)))return static_cast<T>(b);
			throw param_error(i,"float");
		}
	};
	// BOOL
	template <>
	struct X<bool,false,false>{
		static const char_t m = 'b';
		static const char_t t = m;
		static str_t s(){ return _SC("b"); }
		static void put(HSQUIRRELVM v, bool val){ sq_pushbool(v,val); }
		static bool get(HSQUIRRELVM v, int_t i){ bool_t b; sq_tobool(v, i, &b); return static_cast<bool>(b); }
	};
	// STRING
	template <>
	struct X<cstr_t,false,false>{
		static const char_t m = 's';
		static const char_t t = m;
		static str_t s(){ return _SC("s"); }
		static void put(HSQUIRRELVM v, cstr_t val){ sq_pushstring(v,val,-1); }
		static cstr_t get(HSQUIRRELVM v, int_t i){
			cstr_t s;
			if(SQ_SUCCEEDED(sq_getstring(v, i, &s)))return s;
			throw param_error(i,"string");
		}
	};
	// CLASS
	template <class T>
	struct X<T*,false,false>{
		static const char_t m = 'x';
		static const cstr_t t;
		static ptr_t tag(){ return const_cast<char_t*>(t); }
		static HSQOBJECT id;
		static int_t init(HSQUIRRELVM v){
			if(sq_isclass(id)){
				sq_pushobject(v,id);
			}else{
				sq_newclass(v, false);
				sq_getstackobj(v, -1, &id);
				sq_settypetag(v, -1, tag());
				sq_addref(v, &id);
			}
			return sq_gettop(v);
		}
		static int_t construct(HSQUIRRELVM v, ptr_t p){
			sq_setinstanceup(v, 1, p);
			sq_setreleasehook(v, 1, destructor);
		}
		static int_t destructor(ptr_t p, int_t){ delete static_cast<T*>(p); return 0; }
		static void put(HSQUIRRELVM v, T* val){
			init(v);
			sq_createinstance(v,-1);
			sq_remove(v,-2);
			sq_setinstanceup(v,-1,val);
		}
		static T* get(HSQUIRRELVM v, int_t i){
			ptr_t p=0;
			if(SQ_SUCCEEDED(sq_getinstanceup(v,i,&p,tag())))return static_cast<T*>(p);
			throw param_error(i,typeid(T).name());
		}
	};
	template <class T> const cstr_t X<T*,false,false>::t = typeid(T).name();
	template <class T> HSQOBJECT X<T*,false,false>::id;


//---------------------------------------------------------------------------
//	関数用ヘルパ
//---------------------------------------------------------------------------

	// 戻り値アクセス
	struct Res{
		const HSQUIRRELVM vm;
		const int_t base;
		Res(HSQUIRRELVM v):vm(v), base(sq_gettop(v)){}
		template <class T> operator ()(const T& res){ X<T>::put(vm, res); }
		operator int_t()const{ return sq_gettop(vm) - base; }
	};
	// 引数アクセス
	struct Arg{
		const HSQUIRRELVM vm;
		const int_t size;
		Arg(HSQUIRRELVM v): vm(v), size(sq_gettop(v)){}
		struct Val{
			const HSQUIRRELVM vm;
			const int_t id;
			Val(HSQUIRRELVM v, int_t i): vm(v), id(i){}
			template <class T> operator T()const{ return X<T>::get(vm, id); }
		};
		Val operator[](int_t i)const{
			++i;
			if(size<i)throw param_error(i,"avail");
			return Val(vm, i);
		}
	};

	// 関数用ヘルパ
	template <class F> F GetFunc(HSQUIRRELVM v){
		F f;
		sq_getuserpointer(v, -1, reinterpret_cast<ptr_t*>(&f));
		return f;
	}

	// メンバ関数用ヘルパ
	template <class M>
	M GetMethod(HSQUIRRELVM v){
		M* f;
		sq_getuserdata(v, -1, reinterpret_cast<ptr_t*>(&f), NULL);
		return *f;
	}
	template <class C>
	C* GetInst(HSQUIRRELVM v){
		return X<C*>::get(v, 1);
	}

	// 登録用ヘルパ
	void BindFuncProxy(HSQUIRRELVM v, void* f, SQFUNCTION sf){
		sq_pushuserpointer(v, f);
		sq_newclosure(v, sf, 1);
	}
	void BindMethodProxy(HSQUIRRELVM v, void* m, size_t size, SQFUNCTION sf){
		memcpy(sq_newuserdata(v, size), m, size);
		sq_newclosure(v, sf, 1);
	}


//---------------------------------------------------------------------------
//	引数解析（実行時）
//---------------------------------------------------------------------------
	str_t ArgsSummary(HSQUIRRELVM vm){
		size_t size = sq_gettop(vm);
		stream_t s;
		for(size_t i=2; i<=size; ++i){
			switch(sq_gettype(vm, i)){
			case OT_INTEGER: s << 'n'; continue;
			case OT_FLOAT:   s << 'n'; continue;
			case OT_BOOL:    s << 'b'; continue;
			case OT_STRING:  s << 's'; continue;
			}
			ptr_t tag=0;
			sq_gettypetag(vm,i,&tag);
			if(tag){ s << static_cast<cstr_t>(tag); }
		}
		return s.str();
	}

//---------------------------------------------------------------------------
//	引数解析（登録時）
//---------------------------------------------------------------------------
	// ベース
	str_t ArgsSummaryT(){ return str_t(); }
	template <__C1__> str_t ArgsSummaryT(){ return str_t() + X<A1>::t; }
	template <__C2__> str_t ArgsSummaryT(){ return str_t() + X<A1>::t + X<A2>::t; }
	template <__C3__> str_t ArgsSummaryT(){ return str_t() + X<A1>::t + X<A2>::t + X<A3>::t; }
	template <__C4__> str_t ArgsSummaryT(){ return str_t() + X<A1>::t + X<A2>::t + X<A3>::t + X<A4>::t; }
	template <__C5__> str_t ArgsSummaryT(){ return str_t() + X<A1>::t + X<A2>::t + X<A3>::t + X<A4>::t + X<A5>::t; }
	template <__C6__> str_t ArgsSummaryT(){ return str_t() + X<A1>::t + X<A2>::t + X<A3>::t + X<A4>::t + X<A5>::t + X<A6>::t; }
	template <__C7__> str_t ArgsSummaryT(){ return str_t() + X<A1>::t + X<A2>::t + X<A3>::t + X<A4>::t + X<A5>::t + X<A6>::t + X<A7>::t; }
	template <__C8__> str_t ArgsSummaryT(){ return str_t() + X<A1>::t + X<A2>::t + X<A3>::t + X<A4>::t + X<A5>::t + X<A6>::t + X<A7>::t + X<A8>::t; }

	// 通常関数
	template <class R> str_t ArgsSummary(R(*)()){ return ArgsSummaryT(); }
	template <class R, __C1__> str_t ArgsSummary(R(*)(__A1__)){ return ArgsSummaryT<__A1__>(); }
	template <class R, __C2__> str_t ArgsSummary(R(*)(__A2__)){ return ArgsSummaryT<__A2__>(); }
	template <class R, __C3__> str_t ArgsSummary(R(*)(__A3__)){ return ArgsSummaryT<__A3__>(); }
	template <class R, __C4__> str_t ArgsSummary(R(*)(__A4__)){ return ArgsSummaryT<__A4__>(); }
	template <class R, __C5__> str_t ArgsSummary(R(*)(__A5__)){ return ArgsSummaryT<__A5__>(); }
	template <class R, __C6__> str_t ArgsSummary(R(*)(__A6__)){ return ArgsSummaryT<__A6__>(); }
	template <class R, __C7__> str_t ArgsSummary(R(*)(__A7__)){ return ArgsSummaryT<__A7__>(); }
	template <class R, __C8__> str_t ArgsSummary(R(*)(__A8__)){ return ArgsSummaryT<__A8__>(); }
	// メンバ関数
	template <class C, class R> str_t ArgsSummary(R(C::*)()){ return ArgsSummaryT(); }
	template <class C, class R, __C1__> str_t ArgsSummary(R(C::*)(__A1__)){ return ArgsSummaryT<__A1__>(); }
	template <class C, class R, __C2__> str_t ArgsSummary(R(C::*)(__A2__)){ return ArgsSummaryT<__A2__>(); }
	template <class C, class R, __C3__> str_t ArgsSummary(R(C::*)(__A3__)){ return ArgsSummaryT<__A3__>(); }
	template <class C, class R, __C4__> str_t ArgsSummary(R(C::*)(__A4__)){ return ArgsSummaryT<__A4__>(); }
	template <class C, class R, __C5__> str_t ArgsSummary(R(C::*)(__A5__)){ return ArgsSummaryT<__A5__>(); }
	template <class C, class R, __C6__> str_t ArgsSummary(R(C::*)(__A6__)){ return ArgsSummaryT<__A6__>(); }
	template <class C, class R, __C7__> str_t ArgsSummary(R(C::*)(__A7__)){ return ArgsSummaryT<__A7__>(); }
	template <class C, class R, __C8__> str_t ArgsSummary(R(C::*)(__A8__)){ return ArgsSummaryT<__A8__>(); }
	// メンバ関数（const）
	template <class C, class R> str_t ArgsSummary(R(C::*)()const){ return ArgsSummaryT(); }
	template <class C, class R, __C1__> str_t ArgsSummary(R(C::*)(__A1__)const){ return ArgsSummaryT<__A1__>(); }
	template <class C, class R, __C2__> str_t ArgsSummary(R(C::*)(__A2__)const){ return ArgsSummaryT<__A2__>(); }
	template <class C, class R, __C3__> str_t ArgsSummary(R(C::*)(__A3__)const){ return ArgsSummaryT<__A3__>(); }
	template <class C, class R, __C4__> str_t ArgsSummary(R(C::*)(__A4__)const){ return ArgsSummaryT<__A4__>(); }
	template <class C, class R, __C5__> str_t ArgsSummary(R(C::*)(__A5__)const){ return ArgsSummaryT<__A5__>(); }
	template <class C, class R, __C6__> str_t ArgsSummary(R(C::*)(__A6__)const){ return ArgsSummaryT<__A6__>(); }
	template <class C, class R, __C7__> str_t ArgsSummary(R(C::*)(__A7__)const){ return ArgsSummaryT<__A7__>(); }
	template <class C, class R, __C8__> str_t ArgsSummary(R(C::*)(__A8__)const){ return ArgsSummaryT<__A8__>(); }

//---------------------------------------------------------------------------
//	引数マスク
//---------------------------------------------------------------------------
	// ベース
	str_t ArgsMaskT(){ return str_t("."); }
	template <__C1__> str_t ArgsMaskT(){ return str_t(".") + X<A1>::m; }
	template <__C2__> str_t ArgsMaskT(){ return str_t(".") + X<A1>::m + X<A2>::m; }
	template <__C3__> str_t ArgsMaskT(){ return str_t(".") + X<A1>::m + X<A2>::m + X<A3>::m; }
	template <__C4__> str_t ArgsMaskT(){ return str_t(".") + X<A1>::m + X<A2>::m + X<A3>::m + X<A4>::m; }
	template <__C5__> str_t ArgsMaskT(){ return str_t(".") + X<A1>::m + X<A2>::m + X<A3>::m + X<A4>::m + X<A5>::m; }
	template <__C6__> str_t ArgsMaskT(){ return str_t(".") + X<A1>::m + X<A2>::m + X<A3>::m + X<A4>::m + X<A5>::m + X<A6>::m; }
	template <__C7__> str_t ArgsMaskT(){ return str_t(".") + X<A1>::m + X<A2>::m + X<A3>::m + X<A4>::m + X<A5>::m + X<A6>::m + X<A7>::m; }
	template <__C8__> str_t ArgsMaskT(){ return str_t(".") + X<A1>::m + X<A2>::m + X<A3>::m + X<A4>::m + X<A5>::m + X<A6>::m + X<A7>::m + X<A8>::m; }
	// 通常関数
	template <class R> str_t ArgsMask(R(*)()){ return ArgsMaskT(); }
	template <class R, __C1__> str_t ArgsMask(R(*)(__A1__)){ return ArgsMaskT<__A1__>(); }
	template <class R, __C2__> str_t ArgsMask(R(*)(__A2__)){ return ArgsMaskT<__A2__>(); }
	template <class R, __C3__> str_t ArgsMask(R(*)(__A3__)){ return ArgsMaskT<__A3__>(); }
	template <class R, __C4__> str_t ArgsMask(R(*)(__A4__)){ return ArgsMaskT<__A4__>(); }
	template <class R, __C5__> str_t ArgsMask(R(*)(__A5__)){ return ArgsMaskT<__A5__>(); }
	template <class R, __C6__> str_t ArgsMask(R(*)(__A6__)){ return ArgsMaskT<__A6__>(); }
	template <class R, __C7__> str_t ArgsMask(R(*)(__A7__)){ return ArgsMaskT<__A7__>(); }
	template <class R, __C8__> str_t ArgsMask(R(*)(__A8__)){ return ArgsMaskT<__A8__>(); }
	// メンバ関数
	template <class C, class R> str_t ArgsMask(R(C::*)()){ return ArgsMaskT(); }
	template <class C, class R, __C1__> str_t ArgsMask(R(C::*)(__A1__)){ return ArgsMaskT<__A1__>(); }
	template <class C, class R, __C2__> str_t ArgsMask(R(C::*)(__A2__)){ return ArgsMaskT<__A2__>(); }
	template <class C, class R, __C3__> str_t ArgsMask(R(C::*)(__A3__)){ return ArgsMaskT<__A3__>(); }
	template <class C, class R, __C4__> str_t ArgsMask(R(C::*)(__A4__)){ return ArgsMaskT<__A4__>(); }
	template <class C, class R, __C5__> str_t ArgsMask(R(C::*)(__A5__)){ return ArgsMaskT<__A5__>(); }
	template <class C, class R, __C6__> str_t ArgsMask(R(C::*)(__A6__)){ return ArgsMaskT<__A6__>(); }
	template <class C, class R, __C7__> str_t ArgsMask(R(C::*)(__A7__)){ return ArgsMaskT<__A7__>(); }
	template <class C, class R, __C8__> str_t ArgsMask(R(C::*)(__A8__)){ return ArgsMaskT<__A8__>(); }
	// メンバ関数（const）
	template <class C, class R> str_t ArgsMask(R(C::*)()const){ return ArgsMaskT(); }
	template <class C, class R, __C1__> str_t ArgsMask(R(C::*)(__A1__)const){ return ArgsMaskT<__A1__>(); }
	template <class C, class R, __C2__> str_t ArgsMask(R(C::*)(__A2__)const){ return ArgsMaskT<__A2__>(); }
	template <class C, class R, __C3__> str_t ArgsMask(R(C::*)(__A3__)const){ return ArgsMaskT<__A3__>(); }
	template <class C, class R, __C4__> str_t ArgsMask(R(C::*)(__A4__)const){ return ArgsMaskT<__A4__>(); }
	template <class C, class R, __C5__> str_t ArgsMask(R(C::*)(__A5__)const){ return ArgsMaskT<__A5__>(); }
	template <class C, class R, __C6__> str_t ArgsMask(R(C::*)(__A6__)const){ return ArgsMaskT<__A6__>(); }
	template <class C, class R, __C7__> str_t ArgsMask(R(C::*)(__A7__)const){ return ArgsMaskT<__A7__>(); }
	template <class C, class R, __C8__> str_t ArgsMask(R(C::*)(__A8__)const){ return ArgsMaskT<__A8__>(); }
	// Squirrelネイティブ
	cstr_t ArgsMask(SQFUNCTION){ return 0; }

//---------------------------------------------------------------------------
//	通常関数
//---------------------------------------------------------------------------
	template <class R>
	struct FuncProxyT{
		template <class F> static int_t F0(HSQUIRRELVM v){ try{ return Res(v)( GetFunc<F>(v)() ); }__CATCH__ }
		template <class F> static int_t F1(HSQUIRRELVM v){ try{ Arg a(v); return Res(v)( GetFunc<F>(v)(a[1]) ); }__CATCH__ }
		template <class F> static int_t F2(HSQUIRRELVM v){ try{ Arg a(v); return Res(v)( GetFunc<F>(v)(a[1],a[2]) ); }__CATCH__ }
		template <class F> static int_t F3(HSQUIRRELVM v){ try{ Arg a(v); return Res(v)( GetFunc<F>(v)(a[1],a[2],a[3]) ); }__CATCH__ }
		template <class F> static int_t F4(HSQUIRRELVM v){ try{ Arg a(v); return Res(v)( GetFunc<F>(v)(a[1],a[2],a[3],a[4]) ); }__CATCH__ }
		template <class F> static int_t F5(HSQUIRRELVM v){ try{ Arg a(v); return Res(v)( GetFunc<F>(v)(a[1],a[2],a[3],a[4],a[5]) ); }__CATCH__ }
		template <class F> static int_t F6(HSQUIRRELVM v){ try{ Arg a(v); return Res(v)( GetFunc<F>(v)(a[1],a[2],a[3],a[4],a[5],a[6]) ); }__CATCH__ }
		template <class F> static int_t F7(HSQUIRRELVM v){ try{ Arg a(v); return Res(v)( GetFunc<F>(v)(a[1],a[2],a[3],a[4],a[5],a[6],a[7]) ); }__CATCH__ }
		template <class F> static int_t F8(HSQUIRRELVM v){ try{ Arg a(v); return Res(v)( GetFunc<F>(v)(a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8]) ); }__CATCH__ }
		static int_t FV(HSQUIRRELVM v){ try{ return Res(v)( GetFunc<int_t(*)(Arg&)>(v)(Arg(v)) ); }__CATCH__ }
	};
	template <>
	struct FuncProxyT<void>{
		template <class F> static int_t F0(HSQUIRRELVM v){ try{ GetFunc<F>(v)(); return 0;}__CATCH__ }
		template <class F> static int_t F1(HSQUIRRELVM v){ try{ Arg a(v); GetFunc<F>(v)(a[1]); return 0;}__CATCH__ }
		template <class F> static int_t F2(HSQUIRRELVM v){ try{ Arg a(v); GetFunc<F>(v)(a[1],a[2]); return 0;}__CATCH__ }
		template <class F> static int_t F3(HSQUIRRELVM v){ try{ Arg a(v); GetFunc<F>(v)(a[1],a[2],a[3]); return 0;}__CATCH__ }
		template <class F> static int_t F4(HSQUIRRELVM v){ try{ Arg a(v); GetFunc<F>(v)(a[1],a[2],a[3],a[4]); return 0;}__CATCH__ }
		template <class F> static int_t F5(HSQUIRRELVM v){ try{ Arg a(v); GetFunc<F>(v)(a[1],a[2],a[3],a[4],a[5]); return 0;}__CATCH__ }
		template <class F> static int_t F6(HSQUIRRELVM v){ try{ Arg a(v); GetFunc<F>(v)(a[1],a[2],a[3],a[4],a[5],a[6]); return 0;}__CATCH__ }
		template <class F> static int_t F7(HSQUIRRELVM v){ try{ Arg a(v); GetFunc<F>(v)(a[1],a[2],a[3],a[4],a[5],a[6],a[7]); return 0;}__CATCH__ }
		template <class F> static int_t F8(HSQUIRRELVM v){ try{ Arg a(v); GetFunc<F>(v)(a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8]); return 0;}__CATCH__ }
		static int_t FV(HSQUIRRELVM v){ try{ GetFunc<int_t(*)(Arg&)>(v)(Arg(v)); return 0;}__CATCH__ }
	};
	template <class R> void Register(HSQUIRRELVM v, R f()){ BindFuncProxy(v, f, &FuncProxyT<R>::F0<R (*)()> ); }
	template <class R, __C1__> void Register(HSQUIRRELVM v, R f(__A1__)){ BindFuncProxy(v, f, &FuncProxyT<R>::F1<R(*)(__A1__)> ); }
	template <class R, __C2__> void Register(HSQUIRRELVM v, R f(__A2__)){ BindFuncProxy(v, f, &FuncProxyT<R>::F2<R(*)(__A2__)> ); }
	template <class R, __C3__> void Register(HSQUIRRELVM v, R f(__A3__)){ BindFuncProxy(v, f, &FuncProxyT<R>::F3<R(*)(__A3__)> ); }
	template <class R, __C4__> void Register(HSQUIRRELVM v, R f(__A4__)){ BindFuncProxy(v, f, &FuncProxyT<R>::F4<R(*)(__A4__)> ); }
	template <class R, __C5__> void Register(HSQUIRRELVM v, R f(__A5__)){ BindFuncProxy(v, f, &FuncProxyT<R>::F5<R(*)(__A5__)> ); }
	template <class R, __C6__> void Register(HSQUIRRELVM v, R f(__A6__)){ BindFuncProxy(v, f, &FuncProxyT<R>::F6<R(*)(__A6__)> ); }
	template <class R, __C7__> void Register(HSQUIRRELVM v, R f(__A7__)){ BindFuncProxy(v, f, &FuncProxyT<R>::F7<R(*)(__A7__)> ); }
	template <class R, __C8__> void Register(HSQUIRRELVM v, R f(__A8__)){ BindFuncProxy(v, f, &FuncProxyT<R>::F8<R(*)(__A8__)> ); }
	// Squirrelネイティブ
	void Register(HSQUIRRELVM v, SQFUNCTION f){ sq_newclosure(v, f, 0); }

//---------------------------------------------------------------------------
// メンバ関数
//---------------------------------------------------------------------------
	template <class C, class R>
	struct MethodProxyT{
		template <class M> static int_t F0(HSQUIRRELVM v){ try{ return Res(v)( (GetInst<C>(v)->*GetMethod<M>(v))() );}__CATCH__ }
		template <class M> static int_t F1(HSQUIRRELVM v){ try{ Arg a(v); return Res(v)( (GetInst<C>(v)->*GetMethod<M>(v))(a[1]) );}__CATCH__ }
		template <class M> static int_t F2(HSQUIRRELVM v){ try{ Arg a(v); return Res(v)( (GetInst<C>(v)->*GetMethod<M>(v))(a[1],a[2]) );}__CATCH__ }
		template <class M> static int_t F3(HSQUIRRELVM v){ try{ Arg a(v); return Res(v)( (GetInst<C>(v)->*GetMethod<M>(v))(a[1],a[2],a[3]) );}__CATCH__ }
		template <class M> static int_t F4(HSQUIRRELVM v){ try{ Arg a(v); return Res(v)( (GetInst<C>(v)->*GetMethod<M>(v))(a[1],a[2],a[3],a[4]) );}__CATCH__ }
		template <class M> static int_t F5(HSQUIRRELVM v){ try{ Arg a(v); return Res(v)( (GetInst<C>(v)->*GetMethod<M>(v))(a[1],a[2],a[3],a[4],a[5]) );}__CATCH__ }
		template <class M> static int_t F6(HSQUIRRELVM v){ try{ Arg a(v); return Res(v)( (GetInst<C>(v)->*GetMethod<M>(v))(a[1],a[2],a[3],a[4],a[5],a[6]) );}__CATCH__ }
		template <class M> static int_t F7(HSQUIRRELVM v){ try{ Arg a(v); return Res(v)( (GetInst<C>(v)->*GetMethod<M>(v))(a[1],a[2],a[3],a[4],a[5],a[6],a[7]) );}__CATCH__ }
		template <class M> static int_t F8(HSQUIRRELVM v){ try{ Arg a(v); return Res(v)( (GetInst<C>(v)->*GetMethod<M>(v))(a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8]) );}__CATCH__ }
	};
	template <class C>
	struct MethodProxyT<C,void>{
		template <class M> static int_t F0(HSQUIRRELVM v){ try{ (GetInst<C>(v)->*GetMethod<M>(v))(); return 0;}__CATCH__ }
		template <class M> static int_t F1(HSQUIRRELVM v){ try{ Arg a(v); (GetInst<C>(v)->*GetMethod<M>(v))(a[1]); return 0;}__CATCH__ }
		template <class M> static int_t F2(HSQUIRRELVM v){ try{ Arg a(v); (GetInst<C>(v)->*GetMethod<M>(v))(a[1],a[2]); return 0;}__CATCH__ }
		template <class M> static int_t F3(HSQUIRRELVM v){ try{ Arg a(v); (GetInst<C>(v)->*GetMethod<M>(v))(a[1],a[2],a[3]); return 0;}__CATCH__ }
		template <class M> static int_t F4(HSQUIRRELVM v){ try{ Arg a(v); (GetInst<C>(v)->*GetMethod<M>(v))(a[1],a[2],a[3],a[4]); return 0;}__CATCH__ }
		template <class M> static int_t F5(HSQUIRRELVM v){ try{ Arg a(v); (GetInst<C>(v)->*GetMethod<M>(v))(a[1],a[2],a[3],a[4],a[5]); return 0;}__CATCH__ }
		template <class M> static int_t F6(HSQUIRRELVM v){ try{ Arg a(v); (GetInst<C>(v)->*GetMethod<M>(v))(a[1],a[2],a[3],a[4],a[5],a[6]); return 0;}__CATCH__ }
		template <class M> static int_t F7(HSQUIRRELVM v){ try{ Arg a(v); (GetInst<C>(v)->*GetMethod<M>(v))(a[1],a[2],a[3],a[4],a[5],a[6],a[7]); return 0;}__CATCH__ }
		template <class M> static int_t F8(HSQUIRRELVM v){ try{ Arg a(v); (GetInst<C>(v)->*GetMethod<M>(v))(a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8]); return 0;}__CATCH__ }
	};
	template <class C, class R> void Register(HSQUIRRELVM v, R(C::*m)()){ typedef R(C::*M)(); BindMethodProxy(v, &m, sizeof(m), &MethodProxyT<C,R>::F0<M> ); }
	template <class C, class R, __C1__> void Register(HSQUIRRELVM v, R(C::*m)(__A1__)){ typedef R(C::*M)(__A1__); BindMethodProxy(v, &m, sizeof(m), &MethodProxyT<C,R>::F1<M> ); }
	template <class C, class R, __C2__> void Register(HSQUIRRELVM v, R(C::*m)(__A2__)){ typedef R(C::*M)(__A2__); BindMethodProxy(v, &m, sizeof(m), &MethodProxyT<C,R>::F2<M> ); }
	template <class C, class R, __C3__> void Register(HSQUIRRELVM v, R(C::*m)(__A3__)){ typedef R(C::*M)(__A3__); BindMethodProxy(v, &m, sizeof(m), &MethodProxyT<C,R>::F3<M> ); }
	template <class C, class R, __C4__> void Register(HSQUIRRELVM v, R(C::*m)(__A4__)){ typedef R(C::*M)(__A4__); BindMethodProxy(v, &m, sizeof(m), &MethodProxyT<C,R>::F4<M> ); }
	template <class C, class R, __C5__> void Register(HSQUIRRELVM v, R(C::*m)(__A5__)){ typedef R(C::*M)(__A5__); BindMethodProxy(v, &m, sizeof(m), &MethodProxyT<C,R>::F5<M> ); }
	template <class C, class R, __C6__> void Register(HSQUIRRELVM v, R(C::*m)(__A6__)){ typedef R(C::*M)(__A6__); BindMethodProxy(v, &m, sizeof(m), &MethodProxyT<C,R>::F6<M> ); }
	template <class C, class R, __C7__> void Register(HSQUIRRELVM v, R(C::*m)(__A7__)){ typedef R(C::*M)(__A7__); BindMethodProxy(v, &m, sizeof(m), &MethodProxyT<C,R>::F7<M> ); }
	template <class C, class R, __C8__> void Register(HSQUIRRELVM v, R(C::*m)(__A8__)){ typedef R(C::*M)(__A8__); BindMethodProxy(v, &m, sizeof(m), &MethodProxyT<C,R>::F8<M> ); }
	template <class C, class R> void Register(HSQUIRRELVM v, R(C::*m)()const){ typedef R(C::*M)()const; BindMethodProxy(v, &m, sizeof(m), &MethodProxyT<C,R>::F0<M> ); }
	template <class C, class R, __C1__> void Register(HSQUIRRELVM v, R(C::*m)(__A1__)const){ typedef R(C::*M)(__A1__)const; BindMethodProxy(v, &m, sizeof(m), &MethodProxyT<C,R>::F1<M> ); }
	template <class C, class R, __C2__> void Register(HSQUIRRELVM v, R(C::*m)(__A2__)const){ typedef R(C::*M)(__A2__)const; BindMethodProxy(v, &m, sizeof(m), &MethodProxyT<C,R>::F2<M> ); }
	template <class C, class R, __C3__> void Register(HSQUIRRELVM v, R(C::*m)(__A3__)const){ typedef R(C::*M)(__A3__)const; BindMethodProxy(v, &m, sizeof(m), &MethodProxyT<C,R>::F3<M> ); }
	template <class C, class R, __C4__> void Register(HSQUIRRELVM v, R(C::*m)(__A4__)const){ typedef R(C::*M)(__A4__)const; BindMethodProxy(v, &m, sizeof(m), &MethodProxyT<C,R>::F4<M> ); }
	template <class C, class R, __C5__> void Register(HSQUIRRELVM v, R(C::*m)(__A5__)const){ typedef R(C::*M)(__A5__)const; BindMethodProxy(v, &m, sizeof(m), &MethodProxyT<C,R>::F5<M> ); }
	template <class C, class R, __C6__> void Register(HSQUIRRELVM v, R(C::*m)(__A6__)const){ typedef R(C::*M)(__A6__)const; BindMethodProxy(v, &m, sizeof(m), &MethodProxyT<C,R>::F6<M> ); }
	template <class C, class R, __C7__> void Register(HSQUIRRELVM v, R(C::*m)(__A7__)const){ typedef R(C::*M)(__A7__)const; BindMethodProxy(v, &m, sizeof(m), &MethodProxyT<C,R>::F7<M> ); }
	template <class C, class R, __C8__> void Register(HSQUIRRELVM v, R(C::*m)(__A8__)const){ typedef R(C::*M)(__A8__)const; BindMethodProxy(v, &m, sizeof(m), &MethodProxyT<C,R>::F8<M> ); }


//---------------------------------------------------------------------------
//	コンストラクター
//---------------------------------------------------------------------------
	template <class C>
	struct CtorProxyT{
		static int_t F0(HSQUIRRELVM v){ X<C*>::construct(v, new C() ); }
		template <__C1__> static int_t F1(HSQUIRRELVM v){ Arg a(v); X<C*>::construct(v, new C( A1(a[1]) )); }
		template <__C2__> static int_t F2(HSQUIRRELVM v){ Arg a(v); X<C*>::construct(v, new C( A1(a[1]), A2(a[2]) )); }
		template <__C3__> static int_t F3(HSQUIRRELVM v){ Arg a(v); X<C*>::construct(v, new C( A1(a[1]), A2(a[2]), A3(a[3]) )); }
		template <__C4__> static int_t F4(HSQUIRRELVM v){ Arg a(v); X<C*>::construct(v, new C( A1(a[1]), A2(a[2]), A3(a[3]), A4(a[4]) )); }
		template <__C5__> static int_t F5(HSQUIRRELVM v){ Arg a(v); X<C*>::construct(v, new C( A1(a[1]), A2(a[2]), A3(a[3]), A4(a[4]), A5(a[5]) )); }
		template <__C6__> static int_t F6(HSQUIRRELVM v){ Arg a(v); X<C*>::construct(v, new C( A1(a[1]), A2(a[2]), A3(a[3]), A4(a[4]), A5(a[5]), A6(a[6]) )); }
		template <__C7__> static int_t F7(HSQUIRRELVM v){ Arg a(v); X<C*>::construct(v, new C( A1(a[1]), A2(a[2]), A3(a[3]), A4(a[4]), A5(a[5]), A6(a[6]), A7(a[7]) )); }
		template <__C8__> static int_t F8(HSQUIRRELVM v){ Arg a(v); X<C*>::construct(v, new C( A1(a[1]), A2(a[2]), A3(a[3]), A4(a[4]), A5(a[5]), A6(a[6]), A7(a[7]), A8(a[8]) )); }
	};

//---------------------------------------------------------------------------
// バインダー
//---------------------------------------------------------------------------
	struct binder_t{
		binder_t(HSQUIRRELVM vm, int_t target, cstr_t name):
			vm_(vm), base_(sq_gettop(vm)), target_(target), name_(name), overload_(0){}
		const HSQUIRRELVM vm_;
		const int_t base_;
		const int_t target_;
		const cstr_t name_;
		int_t overload_;
		~binder_t(){
			if(overload_){
				sq_pushstring(vm_, name_, -1);
				sq_push(vm_, overload_);
				sq_newclosure(vm_, overload_callback, 1);
				sq_newslot(vm_, target_, false);
			}
		}
		static int_t overload_callback(HSQUIRRELVM vm){
			size_t size = sq_gettop(vm) - 1;
			sq_pushstring(vm, ArgsSummary(vm).c_str(), -1);
			if( SQ_FAILED(sq_get(vm, -2)) )return SQ_ERROR;
			for(size_t i=1; i<=size; ++i){ sq_push(vm, i); }
			sq_call(vm, size, true, true);
			return 1;
		}
		void overload(){
			if(!overload_){ overload_ = newtable(vm_); }
		}

		template <class T>
		binder_t& Bind(T t){
			sq_pushstring(vm_, name_, -1);
			Register(vm_, t);
			sq_newslot(vm_, target_, false);
			return *this;
		}
		template <class T>
		binder_t& Overload(T t){
			overload();
			sq_pushstring(vm_, ArgsSummary(t).c_str(), -1);
			Register(vm_, t);
			sq_newslot(vm_, overload_, false);
			return *this;
		}
		template <class T>
		void Set(const T& val){
			sq_pushstring(vm_,name_,-1);
			X<T>::put(vm_,val);
			sq_set(vm_, target_);
		}
		template <class T>
		T Get(){
			sq_pushstring(vm_,name_,-1);
			if( SQ_FAILED(sq_get(vm_, target_)) )return T();
			return X<T>::get(vm,-1);
		}
		template <class T> binder_t& operator <= (T t){ return Bind<T>(t); }
		template <class T> binder_t& operator << (T t){ return Overload<T>(t); }
		template <class T> operator = (const T& val){ Set<T>(t); }
		template <class T> operator T (){ return Get<T>(); }
	};

	template <class C>
	struct ctor_binder_t:binder_t{
		ctor_binder_t(HSQUIRRELVM vm, int_t target):
			binder_t(vm, target, "constructor")
		{ bind( "", &CtorProxyT<C>::F0); }
		void bind(const str_t& name, SQFUNCTION func){
			overload();
			sq_pushstring(vm_, name.c_str(), -1);
			sq_newclosure(vm_, func, 0);
			sq_newslot(vm_, overload_, false);
		}
		template <__C1__> ctor_binder_t& Ctor(){ bind( ArgsSummaryT<__A1__>(), &CtorProxyT<C>::F1<__A1__>); return *this; }
		template <__C2__> ctor_binder_t& Ctor(){ bind( ArgsSummaryT<__A2__>(), &CtorProxyT<C>::F2<__A2__>); return *this; }
		template <__C3__> ctor_binder_t& Ctor(){ bind( ArgsSummaryT<__A3__>(), &CtorProxyT<C>::F3<__A3__>); return *this; }
		template <__C4__> ctor_binder_t& Ctor(){ bind( ArgsSummaryT<__A4__>(), &CtorProxyT<C>::F4<__A4__>); return *this; }
		template <__C5__> ctor_binder_t& Ctor(){ bind( ArgsSummaryT<__A5__>(), &CtorProxyT<C>::F5<__A5__>); return *this; }
		template <__C6__> ctor_binder_t& Ctor(){ bind( ArgsSummaryT<__A6__>(), &CtorProxyT<C>::F6<__A6__>); return *this; }
		template <__C7__> ctor_binder_t& Ctor(){ bind( ArgsSummaryT<__A7__>(), &CtorProxyT<C>::F7<__A7__>); return *this; }
		template <__C8__> ctor_binder_t& Ctor(){ bind( ArgsSummaryT<__A8__>(), &CtorProxyT<C>::F8<__A8__>); return *this; }
	};

//---------------------------------------------------------------------------
// 配列
//---------------------------------------------------------------------------
	struct array_t{
		const HSQUIRRELVM vm_;
		const int_t self_;
		array_t(HSQUIRRELVM vm, int_t self): vm_(vm), self_(self){}
		~array_t(){ sq_settop(vm_, self_-1); }
		template <class T>
		Push(const T& val){
			X<T>::put(vm_,val);
			sq_arrayappend(vm_, self_);
		}
		void Pop(){ sq_arraypop(vm_, self_, false); }
		void Resize(size_t size){ sq_arrayresize(vm_, self_, size); }
		void Reverse(){ sq_arrayreverse(vm_, self_); }
	};

//---------------------------------------------------------------------------
// テーブル
//---------------------------------------------------------------------------
	struct table_t{
		const HSQUIRRELVM vm_;
		const int_t self_;
		table_t(HSQUIRRELVM vm, int_t self):vm_(vm), self_(self){}
		~table_t(){ sq_settop(vm_, self_-1); }

		friend void Bind(table_t& base, table_t& item, cstr_t name){
				sq_pushstring(base.vm_, name, -1);
				sq_push(base.vm_, item.self_);
				sq_newslot(base.vm_, base.self_, false);
		}
		binder_t operator[](cstr_t name){ return binder_t(vm_, self_, name); }
	};
	// ルートテーブル
	struct root_table_t:table_t{
		root_table_t(HSQUIRRELVM vm):table_t(vm, (sq_pushroottable(vm), sq_gettop(vm))){}
		~root_table_t(){ sq_poptop(vm_); }
	};
	// 定数テーブル
	struct const_table_t:table_t{
		const_table_t(HSQUIRRELVM vm):table_t(vm, (sq_pushconsttable(vm), sq_gettop(vm))){}
		~const_table_t(){ sq_poptop(vm_); }
	};

//---------------------------------------------------------------------------
// クラス
//---------------------------------------------------------------------------
	template <class C>
	struct class_t:table_t{
		class_t(HSQUIRRELVM vm):table_t(vm, X<C*>::init(vm)){}

		ctor_binder_t<C> Ctor(){ return ctor_binder_t<C>(vm_,self_); }
		binder_t operator[](cstr_t name){ return binder_t(vm_, self_, name); }
	};

//---------------------------------------------------------------------------
//	
//---------------------------------------------------------------------------
}
typedef implements::section_t Section;
typedef implements::root_table_t RootTable;
typedef implements::const_table_t ConstTable;
typedef implements::table_t Table;
typedef implements::class_t Class;
}

//---------------------------------------------------------------------------
//	マクロの後始末
//---------------------------------------------------------------------------
#undef SQ_SETPARAMSCHECK
#undef SQ_SETNATIVECLOSURENAME
#undef __C1__
#undef __C2__
#undef __C3__
#undef __C4__
#undef __C5__
#undef __C6__
#undef __C7__
#undef __C8__
#undef __A1__
#undef __A2__
#undef __A3__
#undef __A4__
#undef __A5__
#undef __A6__
#undef __A7__
#undef __A8__
#undef __CATCH__
//---------------------------------------------------------------------------
