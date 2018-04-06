#include <mcl/she.h>
#define CYBOZU_TEST_DISABLE_AUTO_RUN
#include <cybozu/test.hpp>
#include <cybozu/option.hpp>
#include <fstream>

const size_t hashSize = 1 << 10;
const size_t tryNum = 1024;

CYBOZU_TEST_AUTO(init)
{
	int curve;
#if MCLBN_FP_UNIT_SIZE == 4
	curve = MCL_BN254;
#elif MCLBN_FP_UNIT_SIZE == 6
	curve = MCL_BN381_1;
#elif MCLBN_FP_UNIT_SIZE == 8
	curve = MCL_BN462;
#endif
	int ret;
	ret = sheInit(curve, MCLBN_FP_UNIT_SIZE);
	CYBOZU_TEST_EQUAL(ret, 0);
	ret = sheSetRangeForDLP(hashSize);
	CYBOZU_TEST_EQUAL(ret, 0);
}

CYBOZU_TEST_AUTO(encDec)
{
	sheSecretKey sec;
	sheSecretKeySetByCSPRNG(&sec);
	shePublicKey pub;
	sheGetPublicKey(&pub, &sec);

	int64_t m = 123;
	sheCipherTextG1 c1;
	sheCipherTextG2 c2;
	sheCipherTextGT ct;
	sheEncG1(&c1, &pub, m);
	sheEncG2(&c2, &pub, m);
	sheEncGT(&ct, &pub, m);

	int64_t dec;
	CYBOZU_TEST_EQUAL(sheDecG1(&dec, &sec, &c1), 0);
	CYBOZU_TEST_EQUAL(dec, m);
	dec = 0;
	CYBOZU_TEST_EQUAL(sheDecG1ViaGT(&dec, &sec, &c1), 0);
	CYBOZU_TEST_EQUAL(dec, m);
	dec = 0;
	CYBOZU_TEST_EQUAL(sheDecG2(&dec, &sec, &c2), 0);
	CYBOZU_TEST_EQUAL(dec, m);
	dec = 0;
	CYBOZU_TEST_EQUAL(sheDecG2ViaGT(&dec, &sec, &c2), 0);
	CYBOZU_TEST_EQUAL(dec, m);
	dec = 0;
	CYBOZU_TEST_EQUAL(sheDecGT(&dec, &sec, &ct), 0);
	CYBOZU_TEST_EQUAL(dec, m);

	for (int m = -3; m < 3; m++) {
		sheEncG1(&c1, &pub, m);
		CYBOZU_TEST_EQUAL(sheIsZeroG1(&sec, &c1), m == 0);
		sheEncG2(&c2, &pub, m);
		CYBOZU_TEST_EQUAL(sheIsZeroG2(&sec, &c2), m == 0);
		sheEncGT(&ct, &pub, m);
		CYBOZU_TEST_EQUAL(sheIsZeroGT(&sec, &ct), m == 0);
	}
}

CYBOZU_TEST_AUTO(addMul)
{
	sheSecretKey sec;
	sheSecretKeySetByCSPRNG(&sec);
	shePublicKey pub;
	sheGetPublicKey(&pub, &sec);

	int64_t m1 = 12;
	int64_t m2 = -9;
	sheCipherTextG1 c1;
	sheCipherTextG2 c2;
	sheCipherTextGT ct;
	sheEncG1(&c1, &pub, m1);
	sheEncG2(&c2, &pub, m2);
	sheMul(&ct, &c1, &c2);

	int64_t dec;
	CYBOZU_TEST_EQUAL(sheDecGT(&dec, &sec, &ct), 0);
	CYBOZU_TEST_EQUAL(dec, m1 * m2);
}

CYBOZU_TEST_AUTO(allOp)
{
	sheSecretKey sec;
	sheSecretKeySetByCSPRNG(&sec);
	shePublicKey pub;
	sheGetPublicKey(&pub, &sec);

	int64_t m1 = 12;
	int64_t m2 = -9;
	int64_t m3 = 12;
	int64_t m4 = -9;
	sheCipherTextG1 c11, c12;
	sheCipherTextG2 c21, c22;
	sheCipherTextGT ct;
	sheEncG1(&c11, &pub, m1);
	sheEncG1(&c12, &pub, m2);
	sheSubG1(&c11, &c11, &c12); // m1 - m2
	sheMulG1(&c11, &c11, 4); // 4 * (m1 - m2)

	sheEncG2(&c21, &pub, m3);
	sheEncG2(&c22, &pub, m4);
	sheSubG2(&c21, &c21, &c22); // m3 - m4
	sheMulG2(&c21, &c21, -5); // -5 * (m3 - m4)
	sheMul(&ct, &c11, &c21); // -20 * (m1 - m2) * (m3 - m4)
	sheAddGT(&ct, &ct, &ct); // -40 * (m1 - m2) * (m3 - m4)
	sheMulGT(&ct, &ct, -4); // 160 * (m1 - m2) * (m3 - m4)

	int64_t t = 160 * (m1 - m2) * (m3 - m4);
	int64_t dec;
	CYBOZU_TEST_EQUAL(sheDecGT(&dec, &sec, &ct), 0);
	CYBOZU_TEST_EQUAL(dec, t);
}

CYBOZU_TEST_AUTO(rerand)
{
	sheSecretKey sec;
	sheSecretKeySetByCSPRNG(&sec);
	shePublicKey pub;
	sheGetPublicKey(&pub, &sec);

	int64_t m1 = 12;
	int64_t m2 = -9;
	int64_t m3 = 12;
	sheCipherTextG1 c1;
	sheCipherTextG2 c2;
	sheCipherTextGT ct1, ct2;
	sheEncG1(&c1, &pub, m1);
	sheReRandG1(&c1, &pub);

	sheEncG2(&c2, &pub, m2);
	sheReRandG2(&c2, &pub);

	sheEncGT(&ct1, &pub, m3);
	sheReRandGT(&ct1, &pub);

	sheMul(&ct2, &c1, &c2);
	sheReRandGT(&ct2, &pub);
	sheAddGT(&ct1, &ct1, &ct2);

	int64_t dec;
	CYBOZU_TEST_EQUAL(sheDecGT(&dec, &sec, &ct1), 0);
	CYBOZU_TEST_EQUAL(dec, m1 * m2 + m3);
}

CYBOZU_TEST_AUTO(serialize)
{
	sheSecretKey sec1, sec2;
	sheSecretKeySetByCSPRNG(&sec1);
	shePublicKey pub1, pub2;
	sheGetPublicKey(&pub1, &sec1);

	char buf1[2048], buf2[2048];
	size_t n1, n2;
	size_t r, size;
	const size_t sizeofFr = mclBn_getOpUnitSize() * 8;

	size = sizeofFr * 2;
	n1 = sheSecretKeySerialize(buf1, sizeof(buf1), &sec1);
	CYBOZU_TEST_EQUAL(n1, size);
	r = sheSecretKeyDeserialize(&sec2, buf1, n1);
	CYBOZU_TEST_EQUAL(r, n1);
	n2 = sheSecretKeySerialize(buf2, sizeof(buf2), &sec2);
	CYBOZU_TEST_EQUAL(n2, size);
	CYBOZU_TEST_EQUAL_ARRAY(buf1, buf2, n2);

	size = sizeofFr * 3;
	n1 = shePublicKeySerialize(buf1, sizeof(buf1), &pub1);
	CYBOZU_TEST_EQUAL(n1, size);
	r = shePublicKeyDeserialize(&pub2, buf1, n1);
	CYBOZU_TEST_EQUAL(r, n1);
	n2 = shePublicKeySerialize(buf2, sizeof(buf2), &pub2);
	CYBOZU_TEST_EQUAL(n2, size);
	CYBOZU_TEST_EQUAL_ARRAY(buf1, buf2, n2);

	int m = 123;
	sheCipherTextG1 c11, c12;
	sheCipherTextG2 c21, c22;
	sheCipherTextGT ct1, ct2;
	sheEncG1(&c11, &pub2, m);
	sheEncG2(&c21, &pub2, m);
	sheEncGT(&ct1, &pub2, m);

	size = sizeofFr * 2;
	n1 = sheCipherTextG1Serialize(buf1, sizeof(buf1), &c11);
	CYBOZU_TEST_EQUAL(n1, size);
	r = sheCipherTextG1Deserialize(&c12, buf1, n1);
	CYBOZU_TEST_EQUAL(r, n1);
	n2 = sheCipherTextG1Serialize(buf2, sizeof(buf2), &c12);
	CYBOZU_TEST_EQUAL(n2, size);
	CYBOZU_TEST_EQUAL_ARRAY(buf1, buf2, n2);

	size = sizeofFr * 4;
	n1 = sheCipherTextG2Serialize(buf1, sizeof(buf1), &c21);
	CYBOZU_TEST_EQUAL(n1, size);
	r = sheCipherTextG2Deserialize(&c22, buf1, n1);
	CYBOZU_TEST_EQUAL(r, n1);
	n2 = sheCipherTextG2Serialize(buf2, sizeof(buf2), &c22);
	CYBOZU_TEST_EQUAL(n2, size);
	CYBOZU_TEST_EQUAL_ARRAY(buf1, buf2, n2);

	size = sizeofFr * 12 * 4;
	n1 = sheCipherTextGTSerialize(buf1, sizeof(buf1), &ct1);
	CYBOZU_TEST_EQUAL(n1, size);
	r = sheCipherTextGTDeserialize(&ct2, buf1, n1);
	CYBOZU_TEST_EQUAL(r, n1);
	n2 = sheCipherTextGTSerialize(buf2, sizeof(buf2), &ct2);
	CYBOZU_TEST_EQUAL(n2, size);
	CYBOZU_TEST_EQUAL_ARRAY(buf1, buf2, n2);
}

CYBOZU_TEST_AUTO(convert)
{
	sheSecretKey sec;
	sheSecretKeySetByCSPRNG(&sec);
	shePublicKey pub;
	sheGetPublicKey(&pub, &sec);
	sheCipherTextGT ct;
	const int64_t m = 123;
	int64_t dec;
	sheCipherTextG1 c1;
	sheEncG1(&c1, &pub, m);
	CYBOZU_TEST_EQUAL(sheDecG1(&dec, &sec, &c1), 0);
	CYBOZU_TEST_EQUAL(dec, 123);
	sheConvertG1(&ct, &pub, &c1);
	dec = 0;
	CYBOZU_TEST_EQUAL(sheDecGT(&dec, &sec, &ct), 0);
	CYBOZU_TEST_EQUAL(dec, 123);

	sheCipherTextG2 c2;
	sheEncG2(&c2, &pub, m);
	CYBOZU_TEST_EQUAL(sheDecG2(&dec, &sec, &c2), 0);
	CYBOZU_TEST_EQUAL(dec, 123);
	sheConvertG2(&ct, &pub, &c2);
	dec = 0;
	CYBOZU_TEST_EQUAL(sheDecGT(&dec, &sec, &ct), 0);
	CYBOZU_TEST_EQUAL(dec, 123);
}

CYBOZU_TEST_AUTO(precomputed)
{
	sheSecretKey sec;
	sheSecretKeySetByCSPRNG(&sec);
	shePublicKey pub;
	sheGetPublicKey(&pub, &sec);
	shePrecomputedPublicKey *ppub = shePrecomputedPublicKeyCreate();
	CYBOZU_TEST_EQUAL(shePrecomputedPublicKeyInit(ppub, &pub), 0);
	const int64_t m = 152;
	sheCipherTextG1 c1;
	sheCipherTextG2 c2;
	sheCipherTextGT ct;
	int64_t dec = 0;
	CYBOZU_TEST_EQUAL(shePrecomputedPublicKeyEncG1(&c1, ppub, m), 0);
	CYBOZU_TEST_EQUAL(sheDecG1(&dec, &sec, &c1), 0);
	CYBOZU_TEST_EQUAL(dec, m);
	dec = 0;
	CYBOZU_TEST_EQUAL(shePrecomputedPublicKeyEncG2(&c2, ppub, m), 0);
	CYBOZU_TEST_EQUAL(sheDecG2(&dec, &sec, &c2), 0);
	CYBOZU_TEST_EQUAL(dec, m);
	dec = 0;
	CYBOZU_TEST_EQUAL(shePrecomputedPublicKeyEncGT(&ct, ppub, m), 0);
	CYBOZU_TEST_EQUAL(sheDecGT(&dec, &sec, &ct), 0);
	CYBOZU_TEST_EQUAL(dec, m);

	shePrecomputedPublicKeyDestroy(ppub);
}

template<class CT, class PK, class encWithZkpFunc, class decFunc, class verifyFunc>
void ZkpBinTest(const sheSecretKey *sec, const PK *pub, encWithZkpFunc encWithZkp, decFunc dec, verifyFunc verify)
{
	CT c;
	sheZkpBin zkp;
	for (int m = 0; m < 2; m++) {
		CYBOZU_TEST_EQUAL(encWithZkp(&c, &zkp, pub, m), 0);
		mclInt mDec;
		CYBOZU_TEST_EQUAL(dec(&mDec, sec, &c), 0);
		CYBOZU_TEST_EQUAL(mDec, m);
		CYBOZU_TEST_EQUAL(verify(pub, &c, &zkp), 1);
		{
			char buf[2048];
			size_t n = sheZkpBinSerialize(buf, sizeof(buf), &zkp);
			CYBOZU_TEST_EQUAL(n, mclBn_getOpUnitSize() * 8 * 4);
			sheZkpBin zkp2;
			size_t r = sheZkpBinDeserialize(&zkp2, buf, n);
			CYBOZU_TEST_EQUAL(r, n);
			CYBOZU_TEST_ASSERT(memcmp(&zkp, &zkp2, n) == 0);
		}
		zkp.d[0].d[0]++;
		CYBOZU_TEST_EQUAL(verify(pub, &c, &zkp), 0);
	}
	CYBOZU_TEST_ASSERT(encWithZkp(&c, &zkp, pub, 2) != 0);
}

CYBOZU_TEST_AUTO(ZkpBin)
{
	sheSecretKey sec;
	sheSecretKeySetByCSPRNG(&sec);
	shePublicKey pub;
	sheGetPublicKey(&pub, &sec);

	ZkpBinTest<sheCipherTextG1>(&sec, &pub, sheEncWithZkpBinG1, sheDecG1, sheVerifyZkpBinG1);
	ZkpBinTest<sheCipherTextG2>(&sec, &pub, sheEncWithZkpBinG2, sheDecG2, sheVerifyZkpBinG2);

	shePrecomputedPublicKey *ppub = shePrecomputedPublicKeyCreate();
	CYBOZU_TEST_EQUAL(shePrecomputedPublicKeyInit(ppub, &pub), 0);

	ZkpBinTest<sheCipherTextG1>(&sec, ppub, shePrecomputedPublicKeyEncWithZkpBinG1, sheDecG1, shePrecomputedPublicKeyVerifyZkpBinG1);
	ZkpBinTest<sheCipherTextG2>(&sec, ppub, shePrecomputedPublicKeyEncWithZkpBinG2, sheDecG2, shePrecomputedPublicKeyVerifyZkpBinG2);

	shePrecomputedPublicKeyDestroy(ppub);
}

CYBOZU_TEST_AUTO(finalExp)
{
	sheSecretKey sec;
	sheSecretKeySetByCSPRNG(&sec);
	shePublicKey pub;
	sheGetPublicKey(&pub, &sec);
	const int64_t m11 = 5;
	const int64_t m12 = 7;
	const int64_t m21 = -3;
	const int64_t m22 = 9;
	sheCipherTextG1 c11, c12;
	sheCipherTextG2 c21, c22;
	sheCipherTextGT ct1, ct2;
	sheCipherTextGT ct;
	sheEncG1(&c11, &pub, m11);
	sheEncG1(&c12, &pub, m12);
	sheEncG2(&c21, &pub, m21);
	sheEncG2(&c22, &pub, m22);

	int64_t dec;
	// sheMul = sheMulML + sheFinalExpGT
	sheMul(&ct1, &c11, &c21);
	CYBOZU_TEST_EQUAL(sheDecGT(&dec, &sec, &ct1), 0);
	CYBOZU_TEST_EQUAL(dec, m11 * m21);

	sheMulML(&ct1, &c11, &c21);
	sheFinalExpGT(&ct, &ct1);
	CYBOZU_TEST_EQUAL(sheDecGT(&dec, &sec, &ct), 0);
	CYBOZU_TEST_EQUAL(dec, m11 * m21);

	sheMulML(&ct2, &c12, &c22);
	sheFinalExpGT(&ct, &ct2);
	CYBOZU_TEST_EQUAL(sheDecGT(&dec, &sec, &ct), 0);
	CYBOZU_TEST_EQUAL(dec, m12 * m22);

	/*
		Mul(c11, c21) + Mul(c21, c22)
		= finalExp(ML(c11, c21) + ML(c21, c22))
	*/
	sheAddGT(&ct, &ct1, &ct2);
	sheFinalExpGT(&ct, &ct);
	CYBOZU_TEST_EQUAL(sheDecGT(&dec, &sec, &ct), 0);
	CYBOZU_TEST_EQUAL(dec, (m11 * m21) + (m12 * m22));
}

int g_hashBitSize = 8;
std::string g_tableName;

CYBOZU_TEST_AUTO(saveLoad)
{
	sheSecretKey sec;
	sheSecretKeySetByCSPRNG(&sec);
	shePublicKey pub;
	sheGetPublicKey(&pub, &sec);
	const size_t hashSize = 1 << g_hashBitSize;
	const size_t byteSizePerEntry = 8;
	sheSetRangeForGTDLP(hashSize);
	std::string buf;
	buf.resize(hashSize * byteSizePerEntry + 1024);
	const size_t n1 = sheSaveTableForGTDLP(&buf[0], buf.size());
	CYBOZU_TEST_ASSERT(n1 > 0);
	if (!g_tableName.empty()) {
		printf("use table=%s\n", g_tableName.c_str());
		std::ofstream ofs(g_tableName.c_str(), std::ios::binary);
		ofs.write(buf.c_str(), n1);
	}
	const int64_t m = hashSize - 1;
	sheCipherTextGT ct;
	CYBOZU_TEST_ASSERT(sheEncGT(&ct, &pub, m) == 0);
	sheSetRangeForGTDLP(1);
	sheSetTryNum(1);
	int64_t dec = 0;
	CYBOZU_TEST_ASSERT(sheDecGT(&dec, &sec, &ct) != 0);
	if (!g_tableName.empty()) {
		std::ifstream ifs(g_tableName.c_str(), std::ios::binary);
		buf.clear();
		buf.resize(n1);
		ifs.read(&buf[0], n1);
	}
	const size_t n2 = sheLoadTableForGTDLP(&buf[0], n1);
	CYBOZU_TEST_ASSERT(n2 > 0);
	CYBOZU_TEST_ASSERT(sheDecGT(&dec, &sec, &ct) == 0);
	CYBOZU_TEST_EQUAL(dec, m);
}

int main(int argc, char *argv[])
	try
{
	cybozu::Option opt;
	opt.appendOpt(&g_hashBitSize, 8, "bit", ": hashBitSize");
	opt.appendOpt(&g_tableName, "", "f", ": table name");
	opt.appendHelp("h", ": show this message");
	if (!opt.parse(argc, argv)) {
		opt.usage();
		return 1;
	}
	return cybozu::test::autoRun.run(argc, argv);
} catch (std::exception& e) {
	printf("ERR %s\n", e.what());
	return 1;
}
