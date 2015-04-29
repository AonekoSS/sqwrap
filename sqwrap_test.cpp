#include <iostream>
#include <stdarg.h>

#include <squirrel.h>
#include <sqstdio.h>
#include <sqstdaux.h>

#include "sqwrap.h"


using std::string;
using std::cout;
using std::endl;

// 通常関数のテスト用
void TestFunc0(){
	cout << "TestFunc0() is called." << endl; 
}

int TestFunc1(int i){
	cout << "TestFunc1(" << i << ") is called." << endl; 
	return i;
}
string TestFunc2(string a, string b){
	cout << "TestFunc2(" << a << "," << b << ") is called." << endl; 
	return a + b;
}
bool TestFunc3(const char* s){
	cout << "TestFunc3(" << s << ") is called." << endl; 
	return true;
}

// オーバーロードされた関数のテスト用
void OverloadFunc(int i){
	cout << "OverloadFunc(int " << i << ") is called." << endl; 
}
void OverloadFunc(float f){
	cout << "OverloadFunc(float " << f << ") is called." << endl; 
}
void OverloadFunc(string s){
	cout << "OverloadFunc(string " << s << ") is called." << endl; 
}

// クラスのテスト用
class TestClass{
	int member;
public:
	TestClass(const TestClass& c):member(c.member){
		cout << "\tTestClass::TestClass(const TestClass&)" << endl;
	}
	TestClass():member(0){
		cout << "\tTestClass::TestClass()" << endl;
	}
	TestClass(int i):member(i){
		cout << "\tTestClass::TestClass(int " << i << ")" << endl;
	}
	~TestClass(){
		cout << "\tTestClass::~TestClass()" << endl;
	}

	void Method1(int i){
		cout << "TestClass::Method1(" << i << ") is called." << endl;
	}
	int Method2(string s){
		cout << "TestClass::Method2(" << s << ") is called." << endl;
		return s.length();
	}
	int Method3()const{
		cout << "TestClass::Method3() is called." << endl;
		return member;
	}
};

// クラスを受け取る関数
void ClassFunc1(TestClass i){
	cout << "ClassFunc1(" << i.Method3() << ") is called." << endl; 
}
void ClassFunc2(TestClass* i){
	cout << "ClassFunc1(" << i->Method3() << ") is called." << endl; 
}
void ClassFunc3(TestClass& i){
	cout << "ClassFunc1(" << i.Method3() << ") is called." << endl; 
}


// Squirrel からの出力用
void print(HSQUIRRELVM vm, const char* s, ...)  {
	va_list arglist;
	va_start(arglist, s);
	vprintf(s, arglist);
	va_end(arglist);
}


int main(){
	using namespace sqwrap;

	// Squirrel 初期化
	HSQUIRRELVM vm = sq_open(1024);
	sqstd_seterrorhandlers(vm);
	sq_setprintfunc(vm, print, print);

	// ここからバインディング
	{
		RootTable root(vm);

		// 関数をルートテーブルに登録
		root["TestFunc0"] <= TestFunc0;
		root["TestFunc1"] <= TestFunc1;
		root["TestFunc2"] <= TestFunc2;
		root["TestFunc3"] <= TestFunc3;

		// オーバーロード登録
		root["TestFunc"] << TestFunc0 << TestFunc1 << TestFunc2 << TestFunc3; 

		// オーバーロード済みの関数はキャストで個別に取り出して登録
		root["OverloadFunc"]
			<< static_cast<void(*)(int)>(OverloadFunc)
			<< static_cast<void(*)(float)>(OverloadFunc)
			<< static_cast<void(*)(string)>(OverloadFunc);

		// ラムダ式で登録
		root["LamdaA"] <= [](int i){ cout << "LamdaA(" << i << ") is called." << endl; };
		root["LamdaB"] <= [](const string& s){ cout << "LamdaB(" << s << ") is called." << endl; };
			
		// クラスを作る
		{
			Class<TestClass> c(vm);

			// コンストラクターを２種登録
			c().New<>().New<int>();

			// メンバー関数を登録
			c["Method1"] <= &TestClass::Method1;
			c["Method2"] <= &TestClass::Method2;
			c["Method3"] <= &TestClass::Method3;

			// オーバーロードしてみる
			c["Method"] << &TestClass::Method1 << &TestClass::Method2 << &TestClass::Method3;

			// ルートテーブルに登録
			root["TestClass"] <= c;
		}

		// クラスを試す為の関数群
		root["ClassFunc1"] <= ClassFunc1;
		root["ClassFunc2"] <= ClassFunc2;
		root["ClassFunc3"] <= ClassFunc3;
	}

	// 実行
	cout << "＜実行開始＞" << endl;
	sq_pushroottable(vm);
	if(SQ_FAILED(sqstd_dofile(vm, "test.nut", 0, 1))){
		cout << "実行失敗！" << endl;
	}
	cout << "＜実行終了＞" << endl;

	getchar(); // 一時停止

	// 実行後の内容にアクセスしてみる
	{
		RootTable root(vm);

		int Int = root["Int"];
		cout << "Int = " << Int << endl;

		string Str = root["Str"];
		cout << "Str = " << Str << endl;

		Table TableA = root["TableA"];
		int a = TableA["A"];
		int b = TableA["B"];
		cout << "TableA.A = " << a << endl;
		cout << "TableA.B = " << b << endl;
	}

	getchar(); // 一時停止
 	
	// おしまい
	sq_close(vm);
	return 0;
}
