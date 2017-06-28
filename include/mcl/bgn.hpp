#pragma once
/**
	@file
	@brief BGN encryption with prime-order groups
	@author MITSUNARI Shigeo(@herumi)
	@license modified new BSD license
	http://opensource.org/licenses/BSD-3-Clause

	David Mandell Freeman:
	Converting Pairing-Based Cryptosystems from Composite-Order Groups to Prime-Order Groups. EUROCRYPT 2010: 44-61
	http://theory.stanford.edu/~dfreeman/papers/subgroups.pdf

	BGN encryption
	http://theory.stanford.edu/~dfreeman/cs259c-f11/lectures/bgn
*/
#include <vector>
#ifdef MCL_USE_BN384
#include <mcl/bn384.hpp>
#else
#include <mcl/bn256.hpp>
#define MCL_USE_BN256
#endif

namespace mcl { namespace bgn {

namespace local {

struct KeyCount {
	uint32_t key;
	int32_t count; // power
	bool operator<(const KeyCount& rhs) const
	{
		return key < rhs.key;
	}
};

template<class G>
class EcHashTable {
	typedef std::vector<KeyCount> KeyCountVec;
	KeyCountVec kcv;
	G P;
	G nextP;
	int hashSize;
	size_t tryNum;
public:
	EcHashTable() : hashSize(0), tryNum(0) {}
	/*
		compute log_P(xP) for |x| <= hashSize * (tryNum + 1)
	*/
	void init(const G& P, int hashSize, size_t tryNum = 0)
	{
		if (hashSize == 0) throw cybozu::Exception("EcHashTable:init:zero hashSize");
		this->P = P;
		this->hashSize = hashSize;
		this->tryNum = tryNum;
		kcv.resize(hashSize);
		G xP;
		xP.clear();
		for (int i = 1; i <= hashSize; i++) {
			xP += P;
			xP.normalize();
			kcv[i - 1].key = uint32_t(*xP.x.getUnit());
			kcv[i - 1].count = xP.y.isOdd() ? i : -i;
		}
		nextP = xP;
		G::dbl(nextP, nextP);
		nextP += P; // nextP = (hasSize * 2 + 1)P
		/*
			ascending order of abs(count) for same key
		*/
		std::stable_sort(kcv.begin(), kcv.end());
	}
	/*
		log_P(xP)
		find range which has same hash of xP in kcv,
		and detect it
	*/
	int basicLog(G xP, bool *ok = 0) const
	{
		if (ok) *ok = true;
		if (xP.isZero()) return 0;
		typedef KeyCountVec::const_iterator Iter;
		KeyCount kc;
		xP.normalize();
		kc.key = uint32_t(*xP.x.getUnit());
		kc.count = 0;
		std::pair<Iter, Iter> p = std::equal_range(kcv.begin(), kcv.end(), kc);
		G Q;
		Q.clear();
		int prev = 0;
		/*
			check range which has same hash
		*/
		while (p.first != p.second) {
			int count = p.first->count;
			int abs_c = std::abs(count);
			assert(abs_c >= prev); // assume ascending order
			bool neg = count < 0;
			G T;
			G::mul(T, P, abs_c - prev);
			Q += T;
			Q.normalize();
			if (Q.x == xP.x) {
				if (Q.y.isOdd() ^ xP.y.isOdd() ^ neg) return -count;
				return count;
			}
			prev = abs_c;
			++p.first;
		}
		if (ok) {
			*ok = false;
			return 0;
		}
		throw cybozu::Exception("HashTable:basicLog:not found");
	}
	/*
		compute log_P(xP)
		call basicLog at most 2 * tryNum + 1
	*/
	int64_t log(const G& xP) const
	{
		bool ok;
		int c = basicLog(xP, &ok);
		if (ok) {
			return c;
		}
		G posP = xP, negP = xP;
		int64_t posCenter = 0;
		int64_t negCenter = 0;
		int64_t next = hashSize * 2 + 1;
		for (size_t i = 0; i < tryNum; i++) {
			posP -= nextP;
			posCenter += next;
			c = basicLog(posP, &ok);
			if (ok) {
				return posCenter + c;
			}
			negP += nextP;
			negCenter -= next;
			c = basicLog(negP, &ok);
			if (ok) {
				return negCenter + c;
			}
		}
		throw cybozu::Exception("EcHashTable:log:not found");
	}
};

template<class GT>
class GTHashTable {
	typedef std::vector<KeyCount> KeyCountVec;
	KeyCountVec kcv;
	GT g;
	GT nextg;
	GT nextgInv;
	int hashSize;
	size_t tryNum;
public:
	GTHashTable() : hashSize(0), tryNum(0) {}
	/*
		compute log_P(g^x) for |x| <= hashSize * (tryNum + 1)
	*/
	void init(const GT& g, int hashSize, size_t tryNum = 0)
	{
		if (hashSize == 0) throw cybozu::Exception("GTHashTable:init:zero hashSize");
		this->g = g;
		this->hashSize = hashSize;
		this->tryNum = tryNum;
		kcv.resize(hashSize);
		GT gx = 1;
		for (int i = 1; i <= hashSize; i++) {
			gx *= g;
			kcv[i - 1].key = uint32_t(*gx.getFp0()->getUnit());
			kcv[i - 1].count = gx.b.a.a.isOdd() ? i : -i;
		}
		nextg = gx;
		GT::sqr(nextg, nextg);
		nextg *= g; // nextg = g^(hasSize * 2 + 1)
		GT::unitaryInv(nextgInv, nextg);
		/*
			ascending order of abs(count) for same key
		*/
		std::stable_sort(kcv.begin(), kcv.end());
	}
	/*
		log_P(g^x)
		find range which has same hash of gx in kcv,
		and detect it
	*/
	int basicLog(const GT& gx, bool *ok = 0) const
	{
		if (ok) *ok = true;
		if (gx.isOne()) return 0;
		typedef KeyCountVec::const_iterator Iter;
		KeyCount kc;
		kc.key = uint32_t(*gx.getFp0()->getUnit());
		kc.count = 0;
		std::pair<Iter, Iter> p = std::equal_range(kcv.begin(), kcv.end(), kc);
		GT Q = 1;
		int prev = 0;
		/*
			check range which has same hash
		*/
		while (p.first != p.second) {
			int count = p.first->count;
			int abs_c = std::abs(count);
			assert(abs_c >= prev); // assume ascending order
			bool neg = count < 0;
			GT T;
			GT::pow(T, g, abs_c - prev);
			Q *= T;
			if (Q.a == gx.a) {
				if (Q.b.a.a.isOdd() ^ gx.b.a.a.isOdd() ^ neg) return -count;
				return count;
			}
			prev = abs_c;
			++p.first;
		}
		if (ok) {
			*ok = false;
			return 0;
		}
		throw cybozu::Exception("GTHashTable:basicLog:not found");
	}
	/*
		compute log_P(g^x)
		call basicLog at most 2 * tryNum + 1
	*/
	int64_t log(const GT& gx) const
	{
		bool ok;
		int c = basicLog(gx, &ok);
		if (ok) {
			return c;
		}
		GT pos = gx, neg = gx;
		int64_t posCenter = 0;
		int64_t negCenter = 0;
		int64_t next = hashSize * 2 + 1;
		for (size_t i = 0; i < tryNum; i++) {
			pos *= nextgInv;
			posCenter += next;
			c = basicLog(pos, &ok);
			if (ok) {
				return posCenter + c;
			}
			neg *= nextg;
			negCenter -= next;
			c = basicLog(neg, &ok);
			if (ok) {
				return negCenter + c;
			}
		}
		throw cybozu::Exception("GTHashTable:log:not found");
	}
};

template<class G>
int log(const G& P, const G& xP)
{
	if (xP.isZero()) return 0;
	if (xP == P) return 1;
	G negT;
	G::neg(negT, P);
	if (xP == negT) return -1;
	G T = P;
	for (int i = 2; i < 100; i++) {
		T += P;
		if (xP == T) return i;
		G::neg(negT, T);
		if (xP == negT) return -i;
	}
	throw cybozu::Exception("BGN:log:not found");
}

} // mcl::bgn::local

template<class BN, class Fr>
struct BGNT {
	typedef typename BN::G1 G1;
	typedef typename BN::G2 G2;
	typedef typename BN::Fp12 GT;

	class SecretKey;
	class PublicKey;
	// additive HE
	class CipherTextA; // = CipherTextG1 + CipherTextG2
	class CipherTextM; // multiplicative HE
	class CipherText; // CipherTextA + CipherTextM

	static G1 P;
	static G2 Q;

	static inline void init(const mcl::bn::CurveParam& cp = mcl::bn::CurveFp254BNb)
	{
#ifdef MCL_USE_BN256
		mcl::bn256::bn256init(cp);
#endif
#ifdef MCL_USE_BN384
		mcl::bn384::bn384init(cp);
#endif
		BN::hashAndMapToG1(P, "0");
		BN::hashAndMapToG2(Q, "0");
	}

	template<class G>
	class CipherTextAT {
		G S, T;
		friend class SecretKey;
		friend class PublicKey;
		friend class CipherTextA;
	public:
		static inline void add(CipherTextAT& z, const CipherTextAT& x, const CipherTextAT& y)
		{
			/*
				(S, T) + (S', T') = (S + S', T + T')
			*/
			G::add(z.S, x.S, y.S);
			G::add(z.T, x.T, y.T);
		}
		void add(const CipherTextAT& c) { add(*this, *this, c); }
	};

	typedef CipherTextAT<G1> CipherTextG1;
	typedef CipherTextAT<G2> CipherTextG2;

	class SecretKey {
		Fr x1, y1, z1;
		Fr x2, y2, z2;
		G1 B1; // (x1 y1 - z1) P
		G2 B2; // (x2 y2 - z2) Q
		Fr x1x2;
		GT g; // e(B1, B2)
		local::EcHashTable<G1> ecHashTbl;
		local::GTHashTable<GT> gtHashTbl;
	public:
		template<class RG>
		void setByCSPRNG(RG& rg)
		{
			x1.setRand(rg);
			y1.setRand(rg);
			z1.setRand(rg);
			x2.setRand(rg);
			y2.setRand(rg);
			z2.setRand(rg);
			G1::mul(B1, P, x1 * y1 - z1);
			G2::mul(B2, Q, x2 * y2 - z2);
			x1x2 = x1 * x2;
			BN::pairing(g, B1, B2);
		}
		void setDecodeRange(size_t hashSize)
		{
			ecHashTbl.init(B1, hashSize);
			gtHashTbl.init(g, hashSize);
		}
		/*
			set (xP, yP, zP) and (xQ, yQ, zQ)
		*/
		void getPublicKey(PublicKey& pub) const
		{
			G1::mul(pub.xP, P, x1);
			G1::mul(pub.yP, P, y1);
			G1::mul(pub.zP, P, z1);
			G2::mul(pub.xQ, Q, x2);
			G2::mul(pub.yQ, Q, y2);
			G2::mul(pub.zQ, Q, z2);
		}
#if 0
		// log_x(y)
		int log(const GT& x, const GT& y) const
		{
			if (y == 1) return 0;
			if (y == x) return 1;
			GT inv;
			GT::unitaryInv(inv, x);
			if (y == inv) return -1;
			GT t = x;
			for (int i = 2; i < 100; i++) {
				t *= x;
				if (y == t) return i;
				GT::unitaryInv(inv, t);
				if (y == inv) return -i;
			}
			throw cybozu::Exception("BGN:dec:log:not found");
		}
#endif
		int64_t dec(const CipherTextG1& c) const
		{
			/*
				S = myP + rP
				T = mzP + rxP
				R = xS - T = m(xy - z)P = mB
			*/
			G1 R;
			G1::mul(R, c.S, x1);
			R -= c.T;
			return ecHashTbl.log(R);
		}
		int64_t dec(const CipherTextA& c) const
		{
			return dec(c.c1);
		}
		int64_t dec(const CipherTextM& c) const
		{
			/*
				(s, t, u, v) := (e(S, S'), e(S, T'), e(T, S'), e(T, T'))
				s^(xx') v / (t^x u^x')
				= e(xS, x'S') e(xS, -T') e(-T, x'S') e(T, T')
				= e(xS - T, x'S' - T')
				= e(m B1, m' B2)
				= e(B1, B2)^(mm')
			*/
			GT s, t, u;
			GT::pow(s, c.g[0], x1x2);
			s *= c.g[3];
			GT::pow(t, c.g[1], x1);
			GT::pow(u, c.g[2], x2);
			t *= u;
			GT::unitaryInv(t, t);
			s *= t;
			BN::finalExp(s, s);
			return gtHashTbl.log(s);
//			return log(g, s);
		}
		int64_t dec(const CipherText& c) const
		{
			if (c.isMultiplied()) {
				return dec(c.m);
			} else {
				return dec(c.a);
			}
		}
	};

	class PublicKey {
		G1 xP, yP, zP;
		G2 xQ, yQ, zQ;
		friend class SecretKey;
		/*
			(S, T) = (m yP + rP, m zP + r xP)
		*/
		template<class G, class RG>
		static void enc1(G& S, G& T, const G& P, const G& xP, const G& yP, const G& zP, int m, RG& rg)
		{
			Fr r;
			r.setRand(rg);
			G C;
			G::mul(S, yP, m);
			G::mul(C, P, r);
			S += C;
			G::mul(T, zP, m);
			G::mul(C, xP, r);
			T += C;
		}
	public:
		template<class RG>
		void enc(CipherTextA& c, int m, RG& rg) const
		{
			enc1(c.c1.S, c.c1.T, P, xP, yP, zP, m, rg);
			enc1(c.c2.S, c.c2.T, Q, xQ, yQ, zQ, m, rg);
		}
		template<class RG>
		void enc(CipherText& c, int m, RG& rg) const
		{
			c.isMultiplied_ = false;
			enc(c.a, m, rg);
		}
		/*
			convert from CipherTextA to CipherTextM
			cm = ca * Enc(1)
		*/
		void convertCipherText(CipherTextM& cm, const CipherTextA& ca) const
		{
			/*
				Enc(1) = (S, T) = (yP + rP, zP + r xP) = (yP, zP) if r = 0
				cm = ca * (yP, zP)
			*/
			BN::millerLoop(cm.g[0], yP, ca.c2.S);
			BN::millerLoop(cm.g[1], yP, ca.c2.T);
			BN::millerLoop(cm.g[2], zP, ca.c2.S);
			BN::millerLoop(cm.g[3], zP, ca.c2.T);
		}
		void convertCipherText(CipherText& cm, const CipherText& ca) const
		{
			if (ca.isMultiplied()) throw cybozu::Exception("bgn:PublicKey:convertCipherText:already isMultiplied");
			cm.isMultiplied_ = true;
			convertCipherText(cm.m, ca.a);
		}
		/*
			c += Enc(0)
		*/
		template<class RG>
		void rerandomize(CipherTextA& c, RG& rg) const
		{
			CipherTextA c0;
			enc(c0, 0, rg);
			CipherTextA::add(c, c, c0);
		}
		template<class RG>
		void rerandomize(CipherTextM& c, RG& rg) const
		{
			/*
				add Enc(0) * Enc(0)
				(S1, T1) * (S2, T2) = (rP, rxP) * (r'Q, r'xQ)
				replace r <- rr'
				= (r P, rxP) * (Q, xQ)
			*/
			G1 S1, T1;
			Fr r;
			r.setRand(rg);
			G1::mul(S1, P, r);
			G1::mul(T1, xP, r);
			GT e;
			BN::millerLoop(e, S1, Q);
			c.g[0] *= e;
			BN::millerLoop(e, S1, xQ);
			c.g[1] *= e;
			BN::millerLoop(e, T1, Q);
			c.g[2] *= e;
			BN::millerLoop(e, T1, xQ);
			c.g[3] *= e;
		}
		template<class RG>
		void rerandomize(CipherText& c, RG& rg) const
		{
			if (c.isMultiplied()) {
				rerandomize(c.m, rg);
			} else {
				rerandomize(c.a, rg);
			}
		}
	};

	class CipherTextA {
		CipherTextG1 c1;
		CipherTextG2 c2;
		friend class SecretKey;
		friend class PublicKey;
	public:
		static inline void add(CipherTextA& z, const CipherTextA& x, const CipherTextA& y)
		{
			CipherTextG1::add(z.c1, x.c1, y.c1);
			CipherTextG2::add(z.c2, x.c2, y.c2);
		}
		static inline void mul(CipherTextM& z, const CipherTextA& x, const CipherTextA& y)
		{
			/*
				(S1, T1) * (S2, T2) = (e(S1, S2), e(S1, T2), e(T1, S2), e(T1, T2))
				call finalExp at once in decrypting c
			*/
			BN::millerLoop(z.g[0], x.c1.S, y.c2.S);
			BN::millerLoop(z.g[1], x.c1.S, y.c2.T);
			BN::millerLoop(z.g[2], x.c1.T, y.c2.S);
			BN::millerLoop(z.g[3], x.c1.T, y.c2.T);
		}
		void add(const CipherTextA& c) { add(*this, *this, c); }
	};

	class CipherTextM {
		GT g[4];
		friend class SecretKey;
		friend class PublicKey;
		friend class CipherTextA;
	public:
		static inline void add(CipherTextM& z, const CipherTextM& x, const CipherTextM& y)
		{
			/*
				(g[i]) * (g'[i]) = (g[i] * g'[i])
			*/
			for (size_t i = 0; i < 4; i++) {
				GT::mul(z.g[i], x.g[i], y.g[i]);
			}
		}
		void add(const CipherTextM& c) { add(*this, *this, c); }
	};

	class CipherText {
		CipherTextA a;
		CipherTextM m;
		bool isMultiplied_;
		friend class SecretKey;
		friend class PublicKey;
	public:
		CipherText() : isMultiplied_(false) {}
		bool isMultiplied() const { return isMultiplied_; }
		static inline void add(CipherText& z, const CipherText& x, const CipherText& y)
		{
			if (x.isMultiplied() && y.isMultiplied()) {
				z.isMultiplied_ = true;
				CipherTextM::add(z.m, x.m, y.m);
				return;
			}
			if (!x.isMultiplied() && !y.isMultiplied()) {
				z.isMultiplied_ = false;
				CipherTextA::add(z.a, x.a, y.a);
				return;
			}
			throw cybozu::Exception("bgn:CipherText:add:mixed CipherText");
		}
		static inline void mul(CipherText& z, const CipherText& x, const CipherText& y)
		{
			if (x.isMultiplied() || y.isMultiplied()) {
				throw cybozu::Exception("bgn:CipherText:mul:mixed CipherText");
			}
			z.isMultiplied_ = true;
			CipherTextA::mul(z.m, x.a, y.a);
		}
		void add(const CipherText& c) { add(*this, *this, c); }
		void mul(const CipherText& c) { mul(*this, *this, c); }
	};
};

template<class BN, class Fr>
typename BN::G1 BGNT<BN, Fr>::P;

template<class BN, class Fr>
typename BN::G2 BGNT<BN, Fr>::Q;

} } // mcl::bgn

