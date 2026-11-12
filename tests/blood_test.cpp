// Deterministic host simulation test. The world query is a controlled floor;
// actual SP collision/camera behavior is checked separately in the game.
#include "../CoD3Host/blood.cpp"
#include "settings.h"
#include <filesystem>
#include <fstream>
#include <vector>
namespace Guest {
uint8_t* Base=nullptr;
uint32_t AllocatePhysical(uint32_t){return 0x40010000;}
PPCFunc* LookupModule(uint32_t){return nullptr;}
}
extern "C" PPC_FUNC(__imp__sub_82448268){}
static int queries=0,checks=0;bool testMenuOpen=false;
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
PPC_FUNC(FloorTrace){
 ++queries;V a=Read(base,ctx.r4.u32),b=Read(base,ctx.r5.u32);float f=1;
 if(a.z>=0&&b.z<0)f=a.z/(a.z-b.z);
 uint32_t w;memcpy(&w,&f,4);Guest::Write32(base,ctx.r3.u32+32,w);
 Put(base,ctx.r3.u32+16,{0,0,1});
}
void Commit(uint8_t* base,uint64_t at,size_t size){VirtualAlloc(base+at,size,MEM_COMMIT,PAGE_READWRITE);}
int main(){
 uint8_t* base=(uint8_t*)VirtualAlloc(nullptr,0x100000000ull,MEM_RESERVE,PAGE_NOACCESS);CHECK(base);Guest::Base=base;
 Commit(base,0x40000000,0x40000);Commit(base,0x82A20000,0x10000);Commit(base,0x82A90000,0x10000);
 uint64_t slot=Guest::FuncTableBase+(0x82485D38-PPC_CODE_BASE)*2;Commit(base,slot&~0xFFFFull,0x20000);
 *reinterpret_cast<PPCFunc**>(base+slot)=FloorTrace;
 Guest::Write32(base,0x82A2A1C8,0);uint32_t view=0x82A98D90;
 Put(base,view+4480,{-100,0,50});Put(base,view+4496,{1,0,0});Put(base,view+4508,{0,1,0});Put(base,view+4520,{0,0,1});
 Put(base,view+4464,{90,61.9275f,0});
 PPCContext ctx{};Blood::Options o;o.enabled=true;o.persistence=1;Blood::Configure(o);
 Blood::Impact hit;hit.target=0x40000000;hit.valid=true;hit.health=100;hit.damage=30;
 hit.point[0]=0;hit.point[2]=12;hit.direction[0]=1;Guest::Write32(base,hit.target+776,70);
 Blood::After(base,hit);CHECK(Blood::Status().queued==1);
 frameAt=GetTickCount64()-50;Frame(ctx,base);CHECK(Blood::Status().particles>0);CHECK(camera.valid);CHECK(Blood::Active());
 auto initial=particles;
 o.physics=false;Blood::Configure(o);frameAt=GetTickCount64()-50;Frame(ctx,base);CHECK(Blood::Status().particles>0);
 V p{0,0,1},v{10,0,0};Step(p,v,0.1f,false);CHECK(v.z==0&&std::fabs(p.x-1)<0.0001f);
 Step(p,v,0.1f,true);CHECK(v.z<0&&v.x<10);
 o.physics=true;Blood::Configure(o);
 for(int i=0;i<40;i++){frameAt=GetTickCount64()-50;frameAt=GetTickCount64()-50;Frame(ctx,base);CHECK(traces<=TraceLimit);}
 CHECK(Blood::Status().particles==0);
 // Create persistent splatters and verify expiration is independent of particles.
 o.persistence=0;Blood::Configure(o);AddDecal({0,0,0},{0,0,1},5);frameAt=GetTickCount64()-50;Frame(ctx,base);CHECK(Blood::Status().decals>0);
 frameAt=GetTickCount64()-100;Frame(ctx,base);CHECK(Blood::Status().decals>0);
 o.persistence=1;Blood::Configure(o);for(int i=0;i<12;i++){frameAt=GetTickCount64()-100;Frame(ctx,base);}CHECK(Blood::Status().decals==0);
 o.maxDecals=8;Blood::Configure(o);for(int i=0;i<100;i++)AddDecal({0,0,0},{0,0,1},5);CHECK(Blood::Status().decals==8);
 o.decals=false;Blood::Configure(o);CHECK(Blood::Status().decals==0);Blood::After(base,hit);frameAt=GetTickCount64()-50;Frame(ctx,base);CHECK(Blood::Status().particles>0);
 o.amount=0;Blood::Configure(o);CHECK(Blood::Status().particles==0&&Blood::Status().queued==0&&!Blood::Active());
 int beforeQueries=queries;frameAt=GetTickCount64()-50;Frame(ctx,base);CHECK(queries==beforeQueries);
 o.amount=200;o.density=3;o.decals=true;Blood::Configure(o);
 for(int i=0;i<1000;i++)Blood::After(base,hit);CHECK(Blood::Status().queued==QueueLimit);frameAt=GetTickCount64()-50;Frame(ctx,base);CHECK(Blood::Status().particles<=ParticleLimit);
 o.enabled=false;Blood::Configure(o);CHECK(Blood::Status().particles==0&&Blood::Status().decals==0&&Blood::Status().queued==0);
 beforeQueries=queries;frameAt=GetTickCount64()-50;Frame(ctx,base);Blood::After(base,hit);CHECK(queries==beforeQueries&&Blood::Status().queued==0);
 // Each persisted control, with non-default values and independent booleans.
 std::filesystem::path dir="blood-test-config";std::filesystem::create_directories(dir);Settings::Load(dir);auto s=Settings::Get();
 s.blood={};s.blood.enabled=true;s.blood.amount=173;s.blood.density=3;s.blood.size=2.25f;s.blood.decals=false;s.blood.physics=false;s.blood.persistence=0;s.blood.maxDecals=47;
 s.blood.color[0]=0.18f;s.blood.color[1]=0.04f;s.blood.color[2]=0.02f;Settings::Set(s);Settings::Load(dir);auto loaded=Settings::Get().blood;
 CHECK(loaded.enabled&&loaded.amount==173&&loaded.density==3);CHECK(loaded.size==2.25f&&!loaded.decals&&!loaded.physics);
 CHECK(loaded.persistence==0&&loaded.maxDecals==47);CHECK(std::fabs(loaded.color[0]-.18f)<.001f&&std::fabs(loaded.color[1]-.04f)<.001f&&std::fabs(loaded.color[2]-.02f)<.001f);
 loaded.amount=999;loaded.density=-4;loaded.maxDecals=Blood::MaxDecals+1;loaded.size=NAN;loaded.color[0]=NAN;Blood::Clamp(loaded);
 CHECK(loaded.amount==200&&loaded.density==0&&loaded.maxDecals==Blood::MaxDecals&&loaded.size==1&&loaded.color[0]==0);
 // World vertices are generated from positions, not cached screen points.
 o.enabled=true;o.amount=100;o.size=1;Blood::Configure(o);Blood::After(base,hit);frameAt=GetTickCount64()-50;Frame(ctx,base);
 std::array<Blood::WorldVertex,8192> vertices;
 CHECK(Blood::BuildWorld(vertices.data(),8192)>0);testMenuOpen=true;CHECK(Blood::BuildWorld(vertices.data(),8192)==0);testMenuOpen=false;
 o.enabled=false;Blood::Configure(o);CHECK(Blood::BuildWorld(vertices.data(),8192)==0);
 // Focused regression: OFF restores original spawning, ON uses experimental variation/directional bias.
 {
  Blood::Impact e{}; e.valid=true; e.damage=30; e.point[0]=0; e.point[1]=0; e.point[2]=12; e.direction[0]=1; e.direction[1]=0; e.direction[2]=0; e.event=42;
  camera.valid=true; camera.eye={-100,0,50};
  options.enabled=true; options.amount=100; options.density=1; options.directionalSprays=false;
  rng=0x93A71B2Du; particles={}; nextParticle=0; queued=0;
  Spawn(e);
  CHECK(particles[0].life>0);
  int classicCount=0; for(auto& p:particles) classicCount+=p.life>0;
  auto classic0=particles[0]; uint32_t classicRng=rng;
  options.directionalSprays=true;
  rng=0x93A71B2Du; particles={}; nextParticle=0; queued=0;
  Spawn(e);
  CHECK(particles[0].life>0);
  bool differs=(particles[0].v.x!=classic0.v.x || particles[0].v.y!=classic0.v.y || particles[0].v.z!=classic0.v.z || particles[0].size!=classic0.size || particles[0].life!=classic0.life || rng!=classicRng);
  CHECK(differs);
  options.directionalSprays=false;
  rng=0x93A71B2Du; particles={}; nextParticle=0; queued=0;
  Spawn(e);
  CHECK(particles[0].v.x==classic0.v.x && particles[0].v.y==classic0.v.y && particles[0].v.z==classic0.v.z && particles[0].size==classic0.size && particles[0].life==classic0.life && rng==classicRng);
 }
 // The two cached outlines share placement and budget. Sweep floor, wall,
 // and sloped normals and confirm ordered, nonintersecting fan geometry.
 {
  Reset("shape test");
  auto shape=Blood::PresetOptions(Blood::Preset::Brutal);
  CHECK(shape.stainVariation);
  shape.enabled=true;Blood::Configure(shape);
  const V normals[]={{0,0,1},{0,-1,0},{.48f,-.64f,.60f}};
  int streaks=0;
  for(V normal:normals){
   for(int j=0;j<32;j++){
    const auto seed=rng;
    const int slot=nextDecal;
    const V travel=Unit(Cross(normal,{1,0,0}))*100;
    AddDecal({12,7,3},normal,5,0,travel);
    CHECK(rng==seed);
    const auto& d=decals[slot];
    const V u=Unit(Cross(d.n,std::fabs(d.n.z)<.9f?V{0,0,1}:V{1,0,0})),v=Cross(d.n,u);
    uint32_t classicSeed=seed;
    for(int i=0;i<12;i++)classicSeed=classicSeed*1664525u+1013904223u;
    const bool streak=((classicSeed^uint32_t(d.order*0x9e3779b9u))&7u)==0;
    streaks+=streak;
    CHECK(d.maskLayer==(streak?2u:1u));
    classicSeed=seed;
    for(int i=0;i<12;i++){
     const V a=d.variedOffsets[i],b=d.variedOffsets[(i+1)%12];
     CHECK(Finite(a)&&Dot(a,a)>=.49f&&Dot(a,a)<=1.001f&&std::fabs(Dot(a,d.n))<.0001f);
     CHECK(Dot(Cross(a,b),d.n)>.04f);
     auto side=[&](V x,V y,V z){return Dot(Cross(y-x,z-x),d.n);};
     for(int j=i+2;j<12;j++){
      if(i==0&&j==11)continue;
      V c=d.variedOffsets[j],e=d.variedOffsets[(j+1)%12];
      auto opposite=[](float x,float y){return (x>1e-6f&&y< -1e-6f)||(y>1e-6f&&x< -1e-6f);};
      CHECK(!(opposite(side(a,b,c),side(a,b,e))&&opposite(side(c,e,a),side(c,e,b))));
     }
     classicSeed=classicSeed*1664525u+1013904223u;
     const float radius=.65f+(classicSeed&255)/255.0f*.35f;
     const float angle=i*6.2831853f/12;
     const V classic=u*(cosf(angle)*radius)+v*(sinf(angle)*radius);
     CHECK(Dot(classic-d.offsets[i],classic-d.offsets[i])==0);
    }
   }
  }
  CHECK(streaks>0&&streaks<96);
  CHECK(Dot(decals[0].variedOffsets[0]-decals[1].variedOffsets[0],decals[0].variedOffsets[0]-decals[1].variedOffsets[0])>.0001f);
  camera.valid=true;
  std::array<Blood::WorldVertex,36> varied{},again{},classic{};
  CHECK(Blood::BuildWorld(varied.data(),36)==36&&varied[0].mask==MaskVertex(128,128,decals[0].maskLayer));
  camera.eye={5,7,9};
  CHECK(Blood::BuildWorld(again.data(),36)==36&&memcmp(varied.data(),again.data(),sizeof varied)==0);
  shape.stainVariation=false;Blood::Configure(shape);
  CHECK(Blood::DetectPreset(shape)==Blood::Preset::Brutal);
  CHECK(Blood::BuildWorld(classic.data(),36)==36&&(classic[0].mask>>16)==0&&memcmp(varied.data(),classic.data(),sizeof varied)!=0);
  shape.stainVariation=true;Blood::Configure(shape);
  CHECK(Blood::BuildWorld(again.data(),36)==36&&memcmp(varied.data(),again.data(),sizeof varied)==0);
  CHECK(Blood::BuildWorld(again.data(),35)==0);
  for(int i=0;i<Blood::MaxDecals+1;i++)AddDecal({0,0,0},{0,0,1},5);
  CHECK(Blood::Status().decals==Blood::MaxDecals);
  std::vector<Blood::WorldVertex> full(Blood::MaxVertices);
  CHECK(Blood::BuildWorld(full.data(),int(full.size()))==Blood::MaxDecals*Blood::VerticesPerSplat);
  s.blood=shape;s.blood.stainVariation=false;s.blood.directionalSprays=true;
  Settings::Set(s);Settings::Load(dir);
  CHECK(!Settings::Get().blood.stainVariation&&Settings::Get().blood.directionalSprays);
  {std::ofstream oldIni(dir/"CoD3Recomp.ini");oldIni<<"blood_amount = 93\n";}
  Settings::Load(dir);
  CHECK(Settings::Get().blood.stainVariation&&Settings::Get().blood.amount==93);
 }
 VirtualFree(base,0,MEM_RELEASE);
 printf("PASS %d blood simulation/settings/world-geometry checks. Native collision stubbed.\n",checks);
}
namespace Overlay { bool IsOpen(){return testMenuOpen;} }
namespace Kernel { void QueueConsoleCommand(const std::string&) {} }
