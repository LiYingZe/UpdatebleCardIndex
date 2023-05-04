import functools
import random
import time
import torch
from torch.utils.data import DataLoader,Dataset
import torch.nn as nn
import torch.nn.functional as F
import math
import torch.optim as optim
import numpy as np

import loadDataSet
import sys
from CITree import tupleList2Page
from CIndex import tuplez, cmp2List, cmp2Listbel
import sys

class MaskedLinear(nn.Linear):
    """ 带有mask的全连接层 """
    def __init__(self, in_features, out_features, bias=True):
        super().__init__(in_features, out_features, bias)
        self.register_buffer('mask', torch.ones(out_features, in_features))# 必须提前注册buffer才能够使用

    def set_mask(self, mask):
        self.mask.data.copy_(torch.from_numpy(mask.astype(np.uint8).T))

    def forward(self, input):
        return F.linear(input, self.mask * self.weight, self.bias)

class MyMADE(nn.Module):
    def __init__(self,Xlength=6,innnerDepth = 3,linearScaleN = 5):
        super(MyMADE, self).__init__()
        innnerDepth=Xlength
        # mk : order
        MK= []
        m0 = []
        m3 = []
        for i in range(Xlength):
            m0.append(i)
            m3.append(i)
        m1 = []
        # m2 = []
        for i in range(innnerDepth):
            m1.append( i)
            # m2.append( random.randint(1,Xlength-1))
        MK.append(m0)
        MK.append(m1)
        # MK.append(m2)
        MK.append(m3)
        # print(m0)
        # print(m1)
        # print(m3)
        self.maskList = []
        iolengthList = [ [ Xlength,innnerDepth], [innnerDepth,Xlength]]
        idx = 0
        for L in iolengthList:
            idx+=1
            i0 = L[0]
            j0 = L[1]
            mask = np.zeros((i0,j0))
            # print(i0,j0)
            for i in range(i0):
                for j in range(j0):
                    maskp0 = MK[idx-1]
                    maskp1 = MK[idx]
                    # print(i,j, maskp0[i], maskp1[j])
                    if maskp0[i] < maskp1[j] and  maskp0[i] >= (maskp1[j]-linearScaleN):
                        mask[i][j] = 1
                    else:
                        mask[i][j] = 0
            self.maskList.append(mask)
        self.fc1 = MaskedLinear(Xlength , innnerDepth)#0-1
        self.fc1.set_mask(self.maskList[0])
        # self.fc2 = MaskedLinear(innnerDepth,innnerDepth)#1-2
        # self.fc2.set_mask(self.maskList[1])
        self.fc3 = MaskedLinear(innnerDepth,Xlength)#2-3
        self.fc3.set_mask(self.maskList[1])
        # self.cachex = None
        # self.cachex2 = None
        # self.cachex3 = None
    def forward(self,x):
        a,b = x.shape
        x = x.view(a,-1).float()
        # print(x.shape)
        x = F.relu(self.fc1(x))
        # self.cachex = x
        # x = F.relu(self.fc2(x))
        # self.cachex2 = x
        x = torch.sigmoid(self.fc3(x))
        # print(x.shape)
        return x


class MyMLP(nn.Module):
    def __init__(self):
        """
        把MADE的输出迁移到桶的输出中
        """
        super(MyMLP,self).__init__()
        self.linear1 = nn.Linear(6,12)
        self.linear2 = nn.Linear(12,6)

    def forward(self,x):
        x = self.linear1(x)
        x = F.relu(x)
        x = self.linear2(x)
        x = torch.sigmoid(x)
        return x

def ExploreR(D):
    D = np.array(D)
    r, c = D.shape
    print(r,c)
    for i in range(c):
        print("col ",i," ndv:",len(set(D[:,i])))
def testIndexCap():
    ZD = np.load('./data/power-ZD.npy')
    r,c = ZD.shape
    tl=[]
    net = torch.load("./Model/MADE.pt").cuda()
    for i in range(r):
        zxi = ZD[i,:]
        tuplezx = tuplez(None, zxi,0)
        tl.append(tuplezx)
    tl = sorted(tl, key=functools.cmp_to_key(cmp2List))
    print("sorted")
    cdfDistance = []
    for i in range(r):
        if i % 1000==0:
            out = net(torch.tensor(tl[i].z).cuda().view(1,-1))
            acc=1
            cdf=0
            for j in range(c-1):
                oneProb = out[0,j]
                v = tl[i].z[j]
                cdf += (acc * (1 - oneProb) * v)
                acc *= (oneProb * v + (1 - oneProb) * (1 - v))
            print("Real cdf:", i / r,"est",cdf)
            cdfDistance.append(abs((i/r)-cdf))
    print('maxdis:',max(cdfDistance),'mindis',min(cdfDistance),'avgdis',sum(cdfDistance)/len(cdfDistance))

def trainMADE(ZD,traiingIter = 10,dataname=None,link=30,lastM=None):
    # ZD = np.load(zdpath)
    # net = torch.load('./Model/MADE.pt')
    r, c = ZD.shape
    # ZD = torch.tensor(ZD).cuda()
    print(r,c)
    if lastM == None:
        net = MyMADE(c, c,link).cuda()
    else:
        net = torch.load(lastM)
    ZD = loadDataSet.datainBinary(ZD)
    batchS = 1024
    dataiter = DataLoader(ZD, batch_size=batchS, shuffle=True)
    losfunc = nn.BCELoss()
    optimizer = optim.Adam(net.parameters())
    batch_idx = 0
    for iii in range(traiingIter):
        for (x, target) in (dataiter):
            # r, c = x.shape
            # xnew = torch.cat([torch.zeros((r, 1)).cuda(), x[:, :-1].cuda()], dim=1).cuda()
            optimizer.zero_grad()
            out = net(x.cuda() + 0.0)
            # print(out)
            loss = losfunc(out, x.cuda() + 0.0)
            loss.backward()
            optimizer.step()
            batch_idx += 1
            if batch_idx % 100 == 0:
                print('\r  ',batch_idx,'/', (r*traiingIter)/batchS,' loss',float(loss),end=' ',flush=True)
                # print(out[0,:])
                # print()
                # print(xnew[0,:])
                # print()
                # print(x[0,:])
                # exit(1)
                torch.save(net, './Model/MADE'+dataname+'.pt')
    torch.save(net, './Model/MADE'+dataname+'.pt')


def MADE2File(madePath,outFileName,zdr,zdc,connectlen,leafnum):
    net = torch.load(madePath)
    parameterL = []
    swrite=""
    swrite+=(str(zdr)+" "+str(zdc)+" "+str(connectlen)+" "+str(leafnum))

    swrite+="\n"
    idx = 0
    for name, p in net.named_parameters():
        print(name)
        print(p.shape)
        pa = p.cpu().detach().numpy()
        if 1 in pa.shape:
            pa = pa.reshape(-1)
        parameterL.append(pa.tolist())
        idx += 1
    s0 = ""
    for it in parameterL:
        sit = str(it)
        sit = sit.replace('[', ' ')
        sit = sit.replace(']', '\n')
        sit = sit.replace(',', ' ')
        s0 += sit
        s0 += '\n'
    swrite+=s0
    f = open(outFileName, mode='w')

    f.write(swrite)
    f.close()
def rootPartition(D,ZD,subNum=100,govbit = 20,dataname=None):
    leafNodeTuple = []
    batch_size = 1000
    govBit = govbit
    for i in range(subNum):
        leafNodeTuple.append([])
    minnimumsep = 1.0 / subNum
    r, c = ZD.shape
    r00=r
    rootFile = './Model/MADE'+dataname+'.pt'
    net = torch.load(rootFile)
    ZD = torch.tensor(ZD.copy())
    dataSet = loadDataSet.datainBinary(ZD)
    dataiter = DataLoader(dataSet, batch_size=batch_size, shuffle=False)
    idx = 0
    rx = r
    innnercnt = 0
    # net.eval()
    belongValueList = torch.zeros((r,1)).int()
    startidx=0
    t0 = time.time()
    binaryz64 = torch.zeros((r,1)).long()
    for  i in range(min(64,c)):
        binaryz64 *=2
        binaryz64 += ZD[:,i].view(-1,1)
    binaryz64 = binaryz64.cpu().numpy()
    tmid  = time.time()
    print('zencode calculated',tmid-t0)

    with torch.no_grad():
        for (x, target) in (dataiter):
            print('\r',idx, '/', rx / batch_size, end=' ',flush=True)
            # print(x.shape,x.dtype,sys.getsizeof(x))
            # print(target.shape,target.dtype,sys.getsizeof(target))
            idx += 1
            # t0 = time.time()
            x = x.cuda()
            out = net(x)
            r, c = out.shape
            # 并行化替代
            acc = torch.ones((r, 1)).cuda()
            cdf = torch.zeros((r, 1)).cuda()
            for j in range(govBit - 1):
                v = x[:,j].view(-1, 1).cuda()
                oneProb = out[:, j].view(-1, 1).cuda()
                cdf += (acc * (1 - oneProb) * v)
                acc *= (oneProb * v + (1 - oneProb) * ( ~ v))

            t1 = time.time()
            x = x.cpu().detach().numpy()
            # print(x[0])
            # exit(1)
            # tx = time.time()
            belongl = (cdf/minnimumsep).int()
            # print(belongl[:10,0])
            # print(belongl[:10, 0].int())
            belongValueList[startidx:startidx+r,:] = belongl
            startidx+=r
            # tuplezx = tuplez((D[innnercnt, :]), x[0,:], cdf[i, 0])
            #
            # for i in range(r):
            #     belong = belongl[i,0]
            #     xi = x[i,:]
            #     tuplezx = tuplez((D[innnercnt, :]), xi, cdf[i, 0])
            #     # print(sys.getsizeof(tuplezx))
            #     # exit(1)
            #     # leafNodeTuple[belong].append(tuplezx)
            #     innnercnt += 1
            t2 = time.time()
            # break
            # print(t1 - t0, t2 - t1, tx - t1)
    print(belongValueList.shape)
    t1 = time.time()
    print(t1-t0)
    tups = np.zeros((r00,2)).astype('int64')
    # tups = []
    for i in range(r00):
        if i%10000==0:
            print('\r',i,'/',r00,end=' ',flush=True)
        tups[i,1] = belongValueList[i,0]
        tups[i,0] = binaryz64[i,0]
        # tups[i,0 ] = i

        # tups.append(tuplez(i,i,belongValueList[i,0],binaryenco32[i,0]))
    # tups = [ tuplez(i,i,belongValueList[i,0],binaryenco32[i,0]) for i in range(r00)]
    # print(tups)
    t2 = time.time()
    print("TL created",t2-t1)
    print('sorting:')
    #debuging
    # tups*=0
    ind = np.lexsort(tups.T)

    # tups = sorted(tups,key=functools.cmp_to_key(cmp2Listbel))
    t3 = time.time()
    # print(tups[ind])
    # exit(1)
    print('sortdone , takes: ',t3-t2)
    # exit(1)
    # print(len(tups))
    # for i in range(10):
    #     print(tups[i].belong)
    r = len(tups)
    curentleafidx =  0
    lasti=0
    subpage = []
    for vi in range(r):
        indi = ind[vi]
        tupbelong = tups[indi,1]
        if tupbelong > curentleafidx:
            print("switching leaf node to ",tupbelong,'old : ',curentleafidx, "leafrecords: ",vi-lasti )
            if (vi- lasti )== 0:
                subpage.append(tuplez(D[indi, :], ZD[indi, :], tups[indi, 1], binaryz64[indi,0]))
                continue
            # print(len(subpage))
            tupleList2Page("N-"+str(curentleafidx), subpage,leafSubName=dataname)
            curentleafidx = int(tupbelong)
            lasti=vi
            subpage=[]
            subpage.append(tuplez(D[indi, :], ZD[indi, :], tups[indi, 1], binaryz64[indi,0]))
        else:
            subpage.append( tuplez(D[  indi, :],ZD[indi,:], tups[indi,1],binaryz64[indi,0]) )
            # print(len(subpage))
    if lasti !=(r-1):
        print("fixing the last one:")
        tupleList2Page("N-" + str(int(curentleafidx)), subpage,leafSubName=dataname)


    print('done')
    return
    # exit(1)
    # for i in range(len(leafNodeTuple)):
    #     if len(leafNodeTuple[i])==0:
    #         continue
    #     print("sorting: leaf",i,"leaf len:",len(leafNodeTuple[i]))
    #     leafNodeTuple[i] = sorted(leafNodeTuple[i],key=functools.cmp_to_key(cmp2List))
    #     print("giving control 2 linear M")
    #     tupleList2Page("N-"+str(i),leafNodeTuple[i])
    #     leafNodeTuple[i]=[]

def trainprocedure(decfile,ZD,trainiter,leafnum,dataname=None,link=30,lastM = None):
    trainStart = time.time()
    if trainiter!=0:
        trainMADE(ZD,trainiter,dataname=dataname,link=link,lastM=lastM)
    # net = torch.load('./Model/MADE.pt')
    trainend = time.time()

    # rootPartition(D, ZD, leafnum, 3,dataname)
    r,c = ZD.shape
    leafnum = leafnum
    connectlen = link
    MADE2File('./Model/MADE'+dataname+'.pt', './Model/MadeRoot'+dataname, r, c, connectlen, leafnum)
    return trainend-trainStart
    # MADE2File('./Model/MADE.pt', './Model/MadeRoot.txt')


def InsertTrain(D,I,trainiter,leafnum,dataname=None,link=30,lastM = None):
    trainStart = time.time()
    if trainiter != 0:
        EffectiveInsMADE(D,I, trainiter, dataname=dataname, link=link, lastM=lastM)
    # net = torch.load('./Model/MADE.pt')
    trainend = time.time()

    # rootPartition(D, ZD, leafnum, 3,dataname)
    

    
    r2,c = I.shape
    if D  is None:
        r1 = 0
    else:
        r1,c = D.shape

    leafnum = leafnum
    connectlen = link
    MADE2File('./Model/MADE' + dataname + '.pt', './Model/MadeRoot' + dataname, r1+r2, c, connectlen, leafnum)
    return trainend - trainStart


def EffectiveInsMADE(D, I, traiingIter=10, dataname=None, link=30, lastM=None):
    '''
    利用局部性原理对训练函数重写
    '''
    r, c = I.shape
    print(r, c)
    if lastM == None:
        net = MyMADE(c, c, link).cuda()
    else:
        net = torch.load(lastM)
        print('Net loaded')
    tdata0 = time.time()
    if D is None:
        print('cold start....')
        D = torch.tensor(I)
        r1 = D.shape[0]
        minr = r1
        idxPerm = torch.randperm(minr)
        idxList = []
        batchS = 1024
        for ix in range( int(minr/batchS) ):
            idxList.append([ix*batchS,min(minr,(ix+1)*batchS)])
        lastv = idxList[-1][1]
        if(lastv < minr):
            idxList.append([lastv,minr])
        losfunc = nn.BCELoss()
        optimizer = optim.Adam(net.parameters())
        batch_idx = 0
        tdata1 = time.time()
        print('data prepare takes:', tdata1 - tdata0)
        ty = time.time()
        for iii in range(traiingIter):
            for startidx0 , endidx0 in idxList:
                # x = I[idxPerm[startidx0:endidx0],:].cuda()
                optimizer.zero_grad()
                # out = net(x.cuda() + 0.0)
                # l1 = losfunc(out, x.cuda() + 0.0) * (r2) / (r1 + r2)
                ipt = D[startidx0:endidx0,:].cuda()
                out = net(ipt + 0.0)
                l2 = losfunc(out, ipt + 0.0) 
                loss = l2
                loss.backward()
                optimizer.step()
                batch_idx += 1
                if batch_idx % 10 == 0:
                    print('\r  ', batch_idx, '/', (r * traiingIter) / batchS, ' loss', float(loss), end=' ', flush=True)
                    # torch.save(net, './Model/MADE'+dataname+'.pt')
        tx = time.time()
        print('\nTraining TKS:', tx - ty)
        
        torch.save(net, './Model/MADE' + dataname + '.pt')
        return 

    D = torch.tensor(D)
    I = torch.tensor(I)
    r1 = D.shape[0]
    r2 = I.shape[0]
    minr = min(r2,r1)
    idxPerm = torch.randperm(minr)
    idxList = []
    batchS = 100000
    for ix in range( int(minr/batchS) ):
        idxList.append([ix*batchS,min(minr,(ix+1)*batchS)])
    lastv = idxList[-1][1]
    if(lastv < minr):
        idxList.append([lastv,minr])
    losfunc = nn.BCELoss()
    optimizer = optim.Adam(net.parameters())
    batch_idx = 0
    tdata1 = time.time()
    print('data prepare takes:', tdata1 - tdata0)
    ty = time.time()
    for iii in range(traiingIter):
        for startidx0 , endidx0 in idxList:
            x = I[idxPerm[startidx0:endidx0],:].cuda()
            optimizer.zero_grad()
            out = net(x.cuda() + 0.0)
            l1 = losfunc(out, x.cuda() + 0.0) * (r2) / (r1 + r2)
            ipt = D[idxPerm[startidx0:endidx0],:].cuda()
            out = net(ipt + 0.0)
            l2 = losfunc(out, ipt + 0.0) * (r1) / (r1 + r2)
            loss = l1 + l2
            loss.backward()
            optimizer.step()
            batch_idx += 1
            if batch_idx % 10 == 0:
                print('\r  ', batch_idx, '/', (r * traiingIter) / batchS, ' loss', float(loss), end=' ', flush=True)
                # torch.save(net, './Model/MADE'+dataname+'.pt')
    tx = time.time()
    print('\nTraining TKS:', tx - ty)

    torch.save(net, './Model/MADE' + dataname + '.pt')


def InsMADE(D,I,traiingIter = 10,dataname=None,link=30,lastM=None):
    r, c = I.shape
    print(r,c)
    if lastM == None:
        net = MyMADE(c, c,link).cuda()
    else:
        net = torch.load(lastM)
        print('Net loaded')
    tdata0 = time.time()
    D = torch.tensor(D).cuda()
    I = torch.tensor(I).cuda()
    ZD = loadDataSet.datainBinary(I)
    ZD2 = loadDataSet.datainBinary(D)
    r1 = D.shape[0]
    r2 = I.shape[0]
    batchS = 10000
    dataiter = DataLoader(ZD, batch_size=batchS, shuffle=True)
    dataiter2x= (DataLoader(ZD2, batch_size=int(batchS), shuffle=True))


    losfunc = nn.BCELoss()
    optimizer = optim.Adam(net.parameters())
    batch_idx = 0
    uploop = len(dataiter)
    iptList = []
    idxx = 0
    for x, t in dataiter2x:
        if(idxx > uploop):
            break
        iptList.append(x.cuda())
    tdata1 = time.time()
    print('data prepare takes:', tdata1 - tdata0)
    ty = time.time()
    for iii in range(traiingIter):
        # ipt = next(iter(dataiter2))[0].cuda()
        innerloop = -1
        for (x, target) in (dataiter):
            optimizer.zero_grad()
            out = net(x.cuda() + 0.0)
            l1 = losfunc(out, x.cuda() + 0.0) *(r2)/(r1+r2)
            ipt = iptList[innerloop]
            if (ipt.shape[0] == batchS):
                loss = l1
            else:
                out = net(ipt + 0.0)
                l2 = losfunc(out, ipt + 0.0) * (r1) / (r1 + r2)
                loss = l1+l2
            loss.backward()
            optimizer.step()
            batch_idx += 1
            if batch_idx % 100 == 0:
                # print(ipt.shape)
                print('\r  ',batch_idx,'/', (r*traiingIter)/batchS,' loss',float(loss),end=' ',flush=True)
                # torch.save(net, './Model/MADE'+dataname+'.pt')
    tx = time.time()
    print('\nTraining Takes:',tx-ty)

    torch.save(net, './Model/MADE'+dataname+'.pt')
if __name__ =="__main__":
    # print('leafsNumber: ',leafs,'Training loops ',trainloops,'data: ',data)
    Logname = sys.argv[1]
    # trainloops = int(sys.argv[2])
    # data = sys.argv[3]
    # print("welcome 2 python env")
    # exit(1)
    trainloops=1
    TraininglogF  = Logname
    OFS = open(TraininglogF, 'w')
    DFRoot = './data/'
    trainT =0
    decfile = DFRoot + 'D' + str(0) + '.txt.npy'
    zfile = DFRoot+"ZD"+str(0)+'.npy'
    ZD = np.load(zfile).astype(bool)
    ZALL = ZD
    # First Train
    t0 = time.time()
    trainprocedure(None, ZALL, 2, leafnum=0,dataname='P'+str(0),lastM=None)
    i=1
    t1 = time.time()
    print("Ins time:", t1 - t0)
    OFS.write(str(t1 - t0) + ',\n')
    # exit(1)
    for j in range(19):
        i=j+1
        decfile = DFRoot + 'D' + str(i) + '.txt.npy'
        zfile = DFRoot + "ZD" + str(i) + '.npy'
        ZD = np.load(zfile).astype(bool)
        t0 = time.time()
        InsertTrain(ZALL,ZD,1,0,dataname='P'+str(i),lastM='./Model/MADE'+'P'+str(j)+'.pt')
        t1 = time.time()
        print("Delta time:",t1-t0)
        ZALL = np.vstack((ZALL, ZD))
        OFS.write(str(t1-t0)+',\n')
        # trainprocedure(None, ZALL, 3, leafnum=leafs, dataname='P' + str(0), lastM=None)
