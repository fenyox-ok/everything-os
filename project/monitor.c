#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdint.h>

#define PORT     5000
#define ESP32_IP "192.168.0.XXX" // Please enter your esp32s password! it will not work without it.

// ── Shared secret (baked in, never transmitted) ───────────────────────────
#define SECRET "ev3ryth1ng-os-k3y-2025"

// ── HMAC-SHA256 (no external deps) ───────────────────────────────────────

#define SHA256_BLOCK  64
#define SHA256_DIGEST 32

typedef struct {
    uint32_t state[8];
    uint64_t count;
    uint8_t  buf[64];
} sha256_ctx;

static const uint32_t K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,
    0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
    0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,
    0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,
    0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
    0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,
    0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,
    0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
    0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

#define ROR32(x,n) (((x)>>(n))|((x)<<(32-(n))))
#define S0(x) (ROR32(x,2)^ROR32(x,13)^ROR32(x,22))
#define S1(x) (ROR32(x,6)^ROR32(x,11)^ROR32(x,25))
#define G0(x) (ROR32(x,7)^ROR32(x,18)^((x)>>3))
#define G1(x) (ROR32(x,17)^ROR32(x,19)^((x)>>10))
#define CH(x,y,z) (((x)&(y))^(~(x)&(z)))
#define MAJ(x,y,z) (((x)&(y))^((x)&(z))^((y)&(z)))

static void sha256_transform(sha256_ctx *ctx, const uint8_t *data) {
    uint32_t W[64], a,b,c,d,e,f,g,h,T1,T2;
    for (int i=0;i<16;i++)
        W[i]=((uint32_t)data[i*4]<<24)|((uint32_t)data[i*4+1]<<16)|
             ((uint32_t)data[i*4+2]<<8)|(uint32_t)data[i*4+3];
    for (int i=16;i<64;i++)
        W[i]=G1(W[i-2])+W[i-7]+G0(W[i-15])+W[i-16];
    a=ctx->state[0]; b=ctx->state[1]; c=ctx->state[2]; d=ctx->state[3];
    e=ctx->state[4]; f=ctx->state[5]; g=ctx->state[6]; h=ctx->state[7];
    for (int i=0;i<64;i++) {
        T1=h+S1(e)+CH(e,f,g)+K[i]+W[i];
        T2=S0(a)+MAJ(a,b,c);
        h=g; g=f; f=e; e=d+T1; d=c; c=b; b=a; a=T1+T2;
    }
    ctx->state[0]+=a; ctx->state[1]+=b; ctx->state[2]+=c; ctx->state[3]+=d;
    ctx->state[4]+=e; ctx->state[5]+=f; ctx->state[6]+=g; ctx->state[7]+=h;
}

static void sha256_init(sha256_ctx *ctx) {
    ctx->count=0;
    ctx->state[0]=0x6a09e667; ctx->state[1]=0xbb67ae85;
    ctx->state[2]=0x3c6ef372; ctx->state[3]=0xa54ff53a;
    ctx->state[4]=0x510e527f; ctx->state[5]=0x9b05688c;
    ctx->state[6]=0x1f83d9ab; ctx->state[7]=0x5be0cd19;
}

static void sha256_update(sha256_ctx *ctx, const uint8_t *data, size_t len) {
    size_t idx = (size_t)(ctx->count & 63);
    ctx->count += len;
    for (size_t i=0; i<len; i++) {
        ctx->buf[idx++] = data[i];
        if (idx==64) { sha256_transform(ctx,ctx->buf); idx=0; }
    }
}

static void sha256_final(sha256_ctx *ctx, uint8_t *digest) {
    uint8_t pad[64]={0}; size_t idx=(size_t)(ctx->count&63);
    pad[0]=0x80;
    size_t padlen = (idx<56)?56-idx:120-idx;
    uint64_t bits=ctx->count*8;
    sha256_update(ctx,pad,padlen);
    uint8_t lb[8];
    for(int i=7;i>=0;i--){lb[i]=(uint8_t)(bits&0xff);bits>>=8;}
    sha256_update(ctx,lb,8);
    for(int i=0;i<8;i++){
        digest[i*4]=(ctx->state[i]>>24)&0xff;
        digest[i*4+1]=(ctx->state[i]>>16)&0xff;
        digest[i*4+2]=(ctx->state[i]>>8)&0xff;
        digest[i*4+3]=ctx->state[i]&0xff;
    }
}

// HMAC-SHA256: produces 8 hex chars (first 4 bytes) as tag
static void hmac_tag(const char *key, const char *msg, char *out) {
    uint8_t ipad[64], opad[64], kbuf[64]={0};
    size_t klen = strlen(key);
    if (klen>64) { sha256_ctx c; sha256_init(&c);
        sha256_update(&c,(uint8_t*)key,klen); sha256_final(&c,kbuf); }
    else memcpy(kbuf,key,klen);
    for(int i=0;i<64;i++){ipad[i]=kbuf[i]^0x36; opad[i]=kbuf[i]^0x5c;}
    sha256_ctx c; uint8_t tmp[32];
    sha256_init(&c); sha256_update(&c,ipad,64);
    sha256_update(&c,(uint8_t*)msg,strlen(msg)); sha256_final(&c,tmp);
    sha256_init(&c); sha256_update(&c,opad,64);
    sha256_update(&c,tmp,32); sha256_final(&c,tmp);
    snprintf(out, 9, "%02x%02x%02x%02x", tmp[0],tmp[1],tmp[2],tmp[3]);
}

// ── Stats ─────────────────────────────────────────────────────────────────

static int cpu_percent() {
    unsigned long long u1,n1,s1,i1,u2,n2,s2,i2;
    FILE *f=fopen("/proc/stat","r");
    if(!f) return 0;
    if(fscanf(f,"cpu %llu %llu %llu %llu",&u1,&n1,&s1,&i1)!=4){fclose(f);return 0;}
    fclose(f);
    usleep(200000);
    f=fopen("/proc/stat","r");
    if(!f) return 0;
    if(fscanf(f,"cpu %llu %llu %llu %llu",&u2,&n2,&s2,&i2)!=4){fclose(f);return 0;}
    fclose(f);
    unsigned long long busy=(u2+n2+s2)-(u1+n1+s1);
    unsigned long long total=busy+(i2-i1);
    return total>0?(int)(busy*100/total):0;
}

static int mem_percent() {
    unsigned long total=1,avail=1; char key[32]; unsigned long val;
    FILE *f=fopen("/proc/meminfo","r"); if(!f) return 0;
    for(int i=0;i<10;i++){
        if(fscanf(f,"%31s %lu kB\n",key,&val)!=2) break;
        if(!strcmp(key,"MemTotal:"))     total=val;
        if(!strcmp(key,"MemAvailable:")) avail=val;
    }
    fclose(f);
    return (int)((total-avail)*100/total);
}

static int bat_percent() {
    int v=-1; FILE *f=fopen("/sys/class/power_supply/BAT0/capacity","r");
    if(!f) return -1;
    if(fscanf(f,"%d",&v)!=1) v=-1;
    fclose(f); return v;
}

static void uptime_hms(char *buf, size_t len) {
    double up=0; FILE *f=fopen("/proc/uptime","r");
    if(!f){strncpy(buf,"00:00:00",len);return;}
    if(fscanf(f,"%lf",&up)!=1) up=0;
    fclose(f);
    int s=(int)up, h=s/3600, m=(s%3600)/60, sec=s%60;
    snprintf(buf,len,"%02d:%02d:%02d",h,m,sec);
}

// ── Main ──────────────────────────────────────────────────────────────────

int main() {
    int sock=socket(AF_INET,SOCK_DGRAM,0); if(sock<0){perror("socket");return 1;}
    int yes=1; setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
    struct sockaddr_in addr; memset(&addr,0,sizeof(addr));
    addr.sin_family=AF_INET; addr.sin_port=htons(PORT);
    addr.sin_addr.s_addr=inet_addr(ESP32_IP);

    char upbuf[12], msg[96], tag[9];
    struct tm *t; time_t now;

    while(1) {
        time(&now); t=localtime(&now);
        uptime_hms(upbuf,sizeof(upbuf));
        // Build payload
        snprintf(msg,sizeof(msg),"%02d:%02d,%d,%d,%d,%s",
            t->tm_hour,t->tm_min,
            cpu_percent(),mem_percent(),bat_percent(),upbuf);
        // Compute and prepend HMAC tag
        hmac_tag(SECRET,msg,tag);
        char pkt[128];
        snprintf(pkt,sizeof(pkt),"%s|%s",tag,msg);
        sendto(sock,pkt,strlen(pkt),0,(struct sockaddr*)&addr,sizeof(addr));
        sleep(1);
    }
}
