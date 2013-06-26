/*
    k15ctl -- AMD Family 15h (aka Bulldozer) P-State, frequency and voltage modification utility
    inspired by k10ctl from Stefan Ziegenbalg

    Copyright (C) 2012 Michael Werner

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 3 as
    published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, see http://www.gnu.org/licenses/.
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <string.h>
#include <math.h>


#define PSTATE_CTL      0xc0010062
#define PSTATE_STATUS   0xc0010063
#define PSTATE_CFG      0xc0010064
#define D18F4           "/proc/bus/pci/00/18.4"
#define D18F5           "/proc/bus/pci/00/18.5"

#define PSTATE_NUM      8
#define NBPSTATE_NUM    2

#define COFVID_CTL      0xc0010070
#define COFVID_STATUS   0xc0010071

#define NBP0            0x160
#define NBP1            0x164

static int boosted_states;

union pstate {
    uint64_t data;
    struct {
        uint32_t cpuFid : 6;
        uint32_t cpuDid : 3;
        uint32_t cpuVid : 7;
        uint32_t raz1 : 6;
        uint32_t nbPstate : 1;
        uint32_t raz2 : 9;
        uint32_t iddValue : 8;
        uint32_t iddDiv : 2;
        uint32_t raz3 : 21;
        uint32_t pstateEn : 1;
     } val;
};

union nbpstate {
    uint32_t data;
    struct {
        uint32_t nbPstateEn : 1;
        uint32_t nbFid : 5;
        uint32_t reserved1 : 1;
        uint32_t nbDid : 1;
        uint32_t reserved2 : 2;
        uint32_t nbVid : 7;
        uint32_t reserved3 : 15;
    } val;
};

union cofid_status {
    uint64_t data;
    struct {
        uint32_t curCpuFid : 6;
        uint32_t curCpuDid : 3;
        uint32_t curCpuVid : 7;
        uint32_t curPstate : 3;
        uint32_t reserved1 : 4;
        uint32_t nbStateDis : 1;
        uint32_t reserved2 : 1;
        uint32_t curNbVid : 7;
        uint32_t startupPstate : 3;
        uint32_t maxVid : 7 ;
        uint32_t minVid : 7;
        uint32_t maxCpuCof : 6;
        uint32_t reserved3 : 1;
        uint32_t curPstateLimit : 3;
        uint32_t maxNbCof : 5;
    } val;
};


static inline int coreCof(union pstate p)
{
    return (100*(p.val.cpuFid+0x10))>>p.val.cpuDid;
}

static inline int nbCof(union nbpstate nbp)
{
    return (200*(nbp.val.nbFid+0x4))>>nbp.val.nbDid;
}


void error(char* msg)
{
  fprintf(stderr, "%s\n", msg);
  exit(EXIT_FAILURE);
}
    

uint64_t rdmsr(int  cpu, uint32_t reg){
    uint64_t data;
    char fdchar[30];
    sprintf(fdchar,"/dev/cpu/%d/msr",cpu);
    int fd = open(fdchar, O_RDONLY);
    if (pread(fd, &data, sizeof data, reg) != sizeof data){
        fprintf(stderr, "Failed to read data from %s\n",fdchar);
        close(fd);
        exit(EXIT_FAILURE);
    }
    close(fd);
    return data;
}

void wrmsr(int cpu, uint32_t reg, uint64_t data){
    char fdchar[30];
    sprintf(fdchar,"/dev/cpu/%d/msr",cpu);
    int fd = open(fdchar, O_WRONLY);
    if (pwrite(fd, &data, sizeof data, reg) != sizeof data){
        fprintf(stderr, "Failed to write data to %s\n",fdchar);
        close(fd);
        exit(EXIT_FAILURE);
    }
    close(fd);
}

uint32_t rdnb(char *pci, uint32_t reg){
    uint32_t data;
    int fd = open(pci, O_RDONLY);
    if (pread(fd, &data, sizeof data, reg) != sizeof data){
        fprintf(stderr, "Failed to read data from northbridge register %s\n",pci);
        close(fd);
        exit(EXIT_FAILURE);
    }
    close(fd);
    return data;
}

void wrnb(char *pci, uint32_t reg, uint32_t data){
    int fd = open(pci, O_WRONLY);
    if (pwrite(fd, &data, sizeof data, reg) != sizeof data){
        fprintf(stderr, "Failed to write data to northbridge register %s\n",pci);
        close(fd);
        exit(EXIT_FAILURE);
    }
    close(fd);
}

void help()
{
    printf("Warning:\n");
    printf("  USE THIS PROGRAM AT YOU OWN RISK. IT MAY DAMAGE YOUR HARDWARE.\n");
    printf("\n");
    printf("Usage: \n");
    printf("  k15ctl -h                                              This help\n");
    printf("  k15ctl -cpu <cpu>[-<cpun>]                             Print a lot of information about CPU(s) <cpu>(-<cpun>)\n");
    printf("  k15ctl -cpu <cpu>[-<cpun>] -p <p-state> [parameters]   Configure the P-State for CPU(s) <cpu>(-<cpun>), see below\n");
    printf("  k15ctl -cpu <cpu>[-<cpun>] -b <bp-state> [parameters]  Configure the Boosted P-State for CPU(s) <cpu>(-<cpun>), see below\n");
    printf("  k15ctl -nb <nb>[-<nbn>] -np <np-state> [parameters]    Configure the P-State for Northbridge(s) <nb>(-<nbn>), see below\n");
    printf("\n");
    printf("Global Parameter:\n");
    printf("  -dry-run         Shows the resulting P-State table without modifying any register.\n");
    printf("\n");
    printf("Parameters for the P-State configuration:\n");
    printf("  -cf <CpuFid>     CPU Frequency ID\n");
    printf("  -cd <CpuDid>     CPU Divisor ID\n");
    printf("  -cv <CpuVid>     CPU Voltage ID\n");
    printf("  -cnp <NbPstate>  Associated Northbridge P-State\n");
    printf("  -pd <mW>         Power dissipation in mW\n");
    printf("  -en <Enable>     Enable or Disable P-State\n");
    printf("\n");
    printf("Parameters for the Northbridge P-State configuration:\n");
    printf("  -nf <NbFid>      Northbridge Frequency ID\n");
    printf("  -nd <NbDid>      Northbridge Divisor ID\n");
    printf("  -nv <NbVid>      Northbridge Voltage ID\n");
    printf("  -nen <Enable>    Enable or Disable Northbridge P-State\n");
    printf("\n");
    printf("Examples: \n");
    printf("  k15ctl -cpu 0-3                                         Print a lot of information about cores 0-3\n");
    printf("  k15ctl -cpu 0-3 -p 0 -nv 46 -cv 22 -cd 0 -cf 16         Set NbVid=46, CpuVid=22, CpuDiD=0, CpuFid=16 for CPUs 0-3 and P-State 0\n");
    printf("  k15ctl -cpu 0-3 -b 1 -cf 22 -nb 0 -np 0 -nf 22 -dry-run Shows the resulting table for boosted P-State 0 on CPUs 0-3 with CpuFid=22 and Northbridge P-State 0 on Northbridge 0 with NbFid=22\n");
    
}


double __getU(int vid)
{  
    if(vid >= 0x7C)
        return 0;
    else
        return 1550 - vid*12.5;
}

double pgetU(union pstate p)
{
    int vid = p.val.cpuVid;
    return __getU(vid);
}

double nbpgetU(union nbpstate nbp)
{
    int vid = nbp.val.nbVid;
    return __getU(vid);
}

double getI(union pstate p){
    double arr[] = {1., 0.1, 0.01, 0.001};
    return p.val.iddValue * arr[p.val.iddDiv];
}

union pstate getPstate(int cpu, int isBoosted, int num)
{
    union pstate p;
    if(isBoosted)
        p.data = rdmsr(cpu, PSTATE_CFG+num);
    else
        p.data = rdmsr(cpu, PSTATE_CFG+num+boosted_states);

    return p;
}

void setPstate(int cpu, int isBoosted, int num, union pstate p)
{
    if(isBoosted)
        wrmsr(cpu, PSTATE_CFG+num, p.data);
    else
        wrmsr(cpu, PSTATE_CFG+num+boosted_states, p.data);
}

union nbpstate getNbpstate(char *pci, int n)
{
    union nbpstate nbp;
    if(n==0)
        nbp.data = rdnb(pci, NBP0);
    else
        nbp.data = rdnb(pci, NBP1);

    return nbp;
}

void setNbpstate(char *pci, int n, union nbpstate nbp)
{
    if(n==0)
        wrnb(pci, NBP0, nbp.data);
    else
        wrnb(pci, NBP1, nbp.data);
}



void __showPstates(int cpu, int dry_run, int dry_num,union pstate dry_p)
{
    int i;
    union pstate p;

    printf("CPU%d\n",cpu);
    printf("\t\t %8s %8s %8s %8s %12s %11s %13s\n","CpuFid","CpuDid","CpuVid","NbPstate", "CpuFreq","UCpu", "PCore");

    for(i = 0; i < PSTATE_NUM; i++){
        p.data = rdmsr(cpu, PSTATE_CFG+i);
        if(dry_run && dry_num==i)
            p = dry_p;
        if(i < boosted_states)
            printf("Boosted P-State %d",i);
        else
            printf("        P-State %d",i-boosted_states);
        if(p.val.pstateEn)
            printf("%8d %8d %8d %8d %8d MHz %8.1f mV %10.2f mW\n",p.val.cpuFid, p.val.cpuDid, p.val.cpuVid, p.val.nbPstate, 
                coreCof(p), pgetU(p), pgetU(p)*getI(p));
        else
            printf("%8s\n","unused");

    }
}

void showPstates(int cpu)
{
    union pstate p;
   __showPstates(cpu, 0, 0, p);
}

void __showNbpstates(char *nb, int dry_run, int dry_num, union nbpstate dry_nbp)
{
    int i;
    union nbpstate nbp;
    printf("Northbridge %s\n",nb);
    printf("\t\t %8s %8s %8s %12s %11s\n","NbFid","NbDid","NbVid", "NbFreq","UNb");
    for(i=0;i<NBPSTATE_NUM;i++) {
        nbp = getNbpstate(nb, i);
        if(dry_run && dry_num == i)
            nbp = dry_nbp;
        printf("NB P-State %d",i);
        if(nbp.val.nbPstateEn)
            printf("\t %8d %8d %8d %8d MHz %8.1f mV\n", nbp.val.nbFid, nbp.val.nbDid, nbp.val.nbVid,
                nbCof(nbp), nbpgetU(nbp)); 
        else
        printf("\t %8s\n","unused");
    }
}

void showNbpstates(char *nb)
{
    union nbpstate nbp;
    __showNbpstates(nb, 0, 0, nbp);
}
    

void setCpu(int cpu, int dry_run, int isBoosted, int p, int cf, int cd, int cv, int np, double pd, int en)
{
    union pstate pstate;

    /* no pstate specified. showing pstate table */
    if(p==-1){
        showPstates(cpu);
        return;
    }

    pstate = getPstate(cpu, isBoosted, p);

    if(cf!=-1)
        pstate.val.cpuFid = cf;
    if(cd!=-1)
        pstate.val.cpuDid = cd;
    if(cv!=-1)
        pstate.val.cpuVid = cv;
    if(np!=-1)
        pstate.val.nbPstate = np;
    if(pd!=-1){
        int iddDiv, iddValue, i, j;
        double old_power = 0, power;
        for(i=0;i<3;i++) /* iddDiv is valid from 0 to 3 */
            for(j=0;j<256;j++){ /*iddValue is valid from 0 to 256 */
                pstate.val.iddDiv=i;
                pstate.val.iddValue=j;
                power = pgetU(pstate) * getI(pstate);
                if(power==pd)
                    break;
                if(fabs(power-pd)<fabs(old_power-pd)){
                    iddDiv = i;
                    iddValue = j;
                    old_power = power;
                }
            }
        if(power!=pd) {
            pstate.val.iddDiv=iddDiv;
            pstate.val.iddValue=iddValue;
            printf("Could not set power dissipation %.2f. Set nearest Value %.2f\n",pd,old_power);
        }
    }
    if(en!=-1)
        pstate.val.pstateEn = en;

    if(dry_run) {
        if(!isBoosted)
            p+=boosted_states;
        __showPstates(cpu, dry_run, p, pstate);
    }
    else
        setPstate(cpu, isBoosted, p, pstate);

}

void setNorthbridge(char *nb, int n, int dry_run, int nf, int nd, int nv, int nben){
    
    if(n==-1){
        showNbpstates(nb);
        return;
    }

    union nbpstate nbp = getNbpstate(nb, n);
        

    if(nf!=-1)
        nbp.val.nbFid = nf;
    if(nd!=-1)
        nbp.val.nbDid = nd;
    if(nv!=-1)
        nbp.val.nbVid = nv;
    if(nben!=-1)
        nbp.val.nbPstateEn = nben;
    
    if(dry_run) {
        __showNbpstates(D18F5, dry_run, n, nbp);
    }
    else
        setNbpstate(D18F5, n, nbp); 
}

void getNorthbridge(int i, char *buf)
{
    int nb0 = 0x18;
    sprintf(buf,"/proc/bus/pci/00/%x.5",nb0+i);
}


int main(int argc,  char** argv){
    int i, c, option_index;
    char *cpus, *nb;
    cpus=nb=0;
    double pd;
    int dry_run=0,p,cf,cd,cv,cnp,en,np,nf,nd,nv,nben; 
    p=cf=cd=cv=cnp=pd=en=np=nf=nd=nv=nben=-1;
    int isBoosted = 0;
    union cofid_status costat;

    if(argc == 1) {
        help();
        exit(EXIT_SUCCESS);
    }

    costat.data = rdmsr(0, COFVID_STATUS);
    int minVid, maxVid;
    minVid = costat.val.minVid;
    maxVid = costat.val.maxVid;
    boosted_states = 0x3 & (rdnb(D18F4, 0x15c)>>2);


    struct option long_options[] = {
        {"cpu", 1, 0, 0},
        {"dry-run", 0, 0, 0},
        {"bp", 1, 0, 0},
        {"p", 1, 0, 0},
        {"cf", 1, 0, 0},
        {"cd", 1, 0, 0},
        {"cv", 1, 0, 0},
        {"cnp", 1, 0, 0},
        {"pd", 1, 0, 0},
        {"en", 1, 0, 0},
        {"nb", 1, 0 ,0},
        {"np", 1, 0, 0},
        {"nf", 1, 0, 0},
        {"nd", 1, 0, 0},
        {"nv", 1, 0, 0},
        {"nben", 1, 0, 0},
        {"h", 0, 0, 0}
    };


    while((c = getopt_long_only(argc, argv, "", long_options, &option_index))!= -1){
        if(c==0)
            switch(option_index){
                case 0:
                    cpus = optarg;
                    break;
                case 1:
                    dry_run = 1;
                    break;
                case 2:
                    if(p!=-1){
                        fprintf(stderr,"Specify boosted P-State or P-State. NOT both.\n");
                        exit(EXIT_FAILURE);
                    }
                    isBoosted = 1;
                    p = atoi(optarg);
                    break;
                case 3:
                    if(p!=-1){
                        fprintf(stderr,"Specify boosted P-State or P-State. NOT both.\n");
                        exit(EXIT_FAILURE);
                    }
                    p = atoi(optarg);
                    break;
                case 4:
                    cf = atoi(optarg);
                    break;
                case 5:
                    cd = atoi(optarg);
                    break;
                case 6:
                    cv = atoi(optarg);
                    if(minVid && cv>minVid){
                        fprintf(stderr, "CpuVid %d is to low. Using minVid %d\n", cv, minVid);
                        cv = minVid;
                    }
                    else if(maxVid && cv<maxVid){
                        fprintf(stderr, "CpuVid %d is to high. Using maxVid %d\n", cv, maxVid);
                        cv = maxVid;
                    }
                    break;
                case 7:
                    cnp = atoi(optarg);
                    break;
                case 8:
                    pd = atof(optarg);
                    break;
                case 9:
                    en = atoi(optarg);
                    break;
                case 10:
                    nb = optarg;
                    break;
                case 11:
                    np = atoi(optarg);
                    break;
                case 12:
                    nf = atoi(optarg);
                    break;
                case 13:
                    nd = atoi(optarg);
                    break;
                case 14:
                    nv = atoi(optarg);
                    break;
                case 15:
                    nben = atoi(optarg);
                    break;
                case 16:
                    help();
                    exit(EXIT_SUCCESS);
                default:
                    fprintf(stderr,"Unrecognized input\n");
                    exit(EXIT_FAILURE);
            }
        else{
            fprintf(stderr,"Wrong or no input\n");
            help();
            exit(EXIT_FAILURE);
        }

    }

    for (i = optind; i < argc; i++){
        printf ("Non-option argument %s\n", argv[i]);
        exit(EXIT_FAILURE);
    }

    if(cpus){
        char *buf = malloc(30);
        char *token;
        int i, begin, end;

        strcpy(buf, cpus);
        token = strtok(buf, "-");
        begin = atoi(token);
        token = strtok(NULL, "-");
        if(token != NULL) {
            end   = atoi(token);
    
            for(i=begin; i<=end; i++){
                setCpu(i, dry_run, isBoosted, p, cf, cd, cv, cnp ,pd, en);
            }
        }
        else {
                setCpu(begin, dry_run, isBoosted, p, cf, cd, cv, cnp ,pd, en);
        }
        free(buf);
    } 
    
    if(nb){
        char *buf = malloc(30);
        char *buf_nb = malloc(30);
        char *token;
        int i, begin, end;

        strcpy(buf, nb);
        token = strtok(buf, "-");
        begin = atoi(token);
        token = strtok(NULL, "-");
        if(token != NULL) {
            end   = atoi(token);
    
            for(i=begin; i<=end; i++){
               getNorthbridge(i, buf_nb);
               setNorthbridge(buf_nb, np, dry_run, nf, nd, nv, nben);
            }
        }
        else {
               getNorthbridge(begin, buf_nb);
               setNorthbridge(buf_nb, np, dry_run, nf, nd, nv, nben);
        }
        free(buf);
        free(buf_nb);
    }
        
    exit(EXIT_SUCCESS);
}
