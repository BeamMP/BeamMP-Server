///
/// Created by Anonymous275 on 7/28/2020
///
#include "Security/Enc.h"
#include "Settings.h"
#include "CustomAssert.h"
//#include <windows.h>
#include "Logger.h"
#include <sstream>
#include <thread>
#include <random>

int Rand(){
    std::random_device r;
    std::default_random_engine e1(r());
    std::uniform_int_distribution<int> uniform_dist(1, 5000);
    return uniform_dist(e1);
}

int log_power(int n,unsigned int p, int mod){
    int result = 1;
    for (; p; p >>= 1u){
        if (p & 1u)result = int((1LL * result * n) % mod);
        n = int((1LL * n * n) % mod);
    }
    return result;
}
bool rabin_miller(int n){
    bool ok = true;
    for (int i = 1; i <= 5 && ok; i++) {
        int a = Rand() + 1;
        int result = log_power(a, n - 1, n);
        ok &= (result == 1);
    }
    return ok;
}
int generate_prime(){
    int generated = Rand();
    while (!rabin_miller(generated))generated = Rand();
    return generated;
}
int gcd(int a, int b){
    while (b){
        int r = a % b;
        a = b;
        b = r;
    }
    return a;
}

int generate_coprime(int n){
    int generated = Rand();
    while (gcd(n, generated) != 1)generated = Rand();
    return generated;
}

std::pair<int, int> euclid_extended(int a, int b) {
    if(!b)return {1, 0};
    auto result = euclid_extended(b, a % b);
    return {result.second, result.first - (a / b) * result.second};
}

int modular_inverse(int n, int mod){
    int inverse = euclid_extended(n, mod).first;
    while(inverse < 0)inverse += mod;
    return inverse;
}

RSA* GenKey(){
    int p, q;
    p = generate_prime();
    q = generate_prime();
    int n = p * q;
    int phi = (p -1) * (q - 1);
    int e = generate_coprime(phi);
    int d = modular_inverse(e, phi);
    return new RSA{n,e,d};
}

int Enc(int value,int e,int n){
    return log_power(value, e, n);
}

int Dec(int value,int d,int n){
    return log_power(value, d, n);
}

#ifdef WIN32
int Handle(EXCEPTION_POINTERS *ep,char* Origin){
    //Assert(false);
    std::stringstream R;
    R << Sec("Code : ") << std::hex
    << ep->ExceptionRecord->ExceptionCode
    << std::dec << Sec(" Origin : ") << Origin;
    except(R.str());
    return 1;
}
#else
// stub
int Handle(EXCEPTION_POINTERS *, char*) { return 1; }
#endif // WIN32

std::string RSA_E(const std::string& Data, RSA*k){
    std::stringstream stream;
    for(const char&c : Data){
        stream << std::hex << Enc(uint8_t(c),k->e,k->n) << "g";
    }
    return stream.str();
}
std::string RSA_E(const std::string& Data,int e, int n){
    std::stringstream stream;
    for(const char&c : Data){
        stream << std::hex << Enc(uint8_t(c),e,n) << "g";
    }
    return stream.str();
}
std::string RSA_D(const std::string& Data, RSA*k){
    std::stringstream ss(Data);
    std::string token,ret;
    while (std::getline(ss, token, 'g')) {
        if(token.find_first_not_of(Sec("0123456789abcdef")) != std::string::npos)return "";
        int c = std::stoi(token, nullptr, 16);
        ret += char(Dec(c,k->d,k->n));
    }
    return ret;
}
