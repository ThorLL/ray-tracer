// random
const float Pi = 3.1416f;
uniform uint FrameCount;
uint RandSeed;
// Random sequence generator
uint LCG(inout uint prev)
{
    const uint LCG_A = 1664525u;
    const uint LCG_C = 1013904223u;
    prev = (LCG_A * prev + LCG_C);
    return prev & 0x00FFFFFFu;
}

void InitRandomSeed()
{
    uint seedTime = FrameCount;
    uint seedX = uint(gl_FragCoord.x);
    uint seedY = uint(gl_FragCoord.y);
    RandSeed = LCG(seedX) ^ LCG(seedY) ^ LCG(seedTime);
}

uint NextRandom()
{
    RandSeed = RandSeed * 747796405u + 2891336453u;
    uint result = ((RandSeed >> ((RandSeed >> 28) + 4u)) ^ RandSeed) * 277803737u;
    result = (result >> 22) ^ result;
    return result;
}

float RandomValue()
{
    return NextRandom() / 4294967295.0; // 2^32 - 1
}
// Random value in normal distribution (with mean=0 and sd=1)
float RandomValueNormalDistribution()
{
    float theta = 2.0f * Pi * RandomValue();
    float rho = sqrt(-2 * log(RandomValue()));
    return rho * cos(theta);
}
// Calculate a random direction
vec3 GetRandomDirection()
{
    float x = RandomValueNormalDistribution();
    float y = RandomValueNormalDistribution();
    float z = RandomValueNormalDistribution();
    return normalize(vec3(x, y , z));
}