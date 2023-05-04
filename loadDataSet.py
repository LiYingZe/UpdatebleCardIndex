import csv
import sys

import pandas as pd
import torch
from torch.utils.data import Dataset, DataLoader
import numpy as np
import xml.sax
import time


def binaryListcmp(first,sec):
    """
    比较第一个是否大于第二个
    """
    leftv = 0
    rightv = 0

    for i in range((min(len(first),len(sec)))):
        if first[i] == sec[i]:
            continue
        else:
            return first[i] > sec[i]
    return 1==1

def loadNumer(file_path='./data/title_sample_10000.csv',smallData = True):

    dataset =None
    if file_path == './data/title_sample_10000.csv':
        col_names = ['kind_id', 'production_year', 'imdb_id', 'episode_of_id', 'season_nr', 'episode_nr']
        col_offsets = [3, 4, 5, 7, 8, 9]
        dataset = []
        with open(file_path) as csvfile:
            reader = csv.reader(csvfile)
            for line in reader:
                if len(line) != 12:  # imdb.title has 12 columns
                    continue
                row = []
                for offset in col_offsets:
                    if line[offset] == '':
                        row.append(0)
                    else:
                        row.append(int(line[offset]))
                dataset.append(row)
        dataset = np.array(dataset)
    elif file_path == './data/power.txt':
        f = open(file_path)
        line = f.readline()
        D = []
        rowcnt = 0
        while line:
            rowcnt+=1
            if smallData ==True:
                if rowcnt > 10000:
                    break
            line = f.readline()
            linecovert = line[:-1].split(';')
            if len(linecovert) !=9:
                continue
            date = linecovert[0].split('/')
            hoursec = linecovert[1].split(':')
            datime = int(date[2]+ date[1]+date[0] + hoursec[0]+hoursec[1] + hoursec[2])
            row = []
            row.append(datime)
            for i in range(6):
                if linecovert[i+2] == '?':
                    row = None
                    break
                row.append(int(float(linecovert[i+2]) * 1000 ) )
            if row == None:
                continue
            else:
                D.append(row)
        D = np.array(D)
        dataset = D
    return dataset


def countBinaryDig(n):
    return int(n).bit_length()
def binaryRow2TokZencode(binaryRowList,tok):
    newEncode = []
    for i in range(len(tok)):
        c,d = tok[i].split('-')
        c = int(c)
        d = int(d)
        newEncode.append( binaryRowList[c][d] )
    return newEncode

def binaryRow2Zencode(binaryRowList):
    newEncode = []
    lenList = []
    for  Listi in binaryRowList:
        lenList.append(len(Listi))
    for  loopIter in range(max(lenList)):
        for Listi in binaryRowList:
            if loopIter >= len(Listi):
                continue
            else:
                newEncode.append(Listi[loopIter])
    return newEncode
def fastbinaryRow2Zencode(binaryRowList,encodinglen,lenList):
    newEncode = np.zeros((encodinglen)).astype(np.bool)
    # lenList = []
    validx=0
    # for  Listi in binaryRowList:
    #     lenList.append(len(Listi))
    for  loopIter in range(max(lenList)):
        for Listi in binaryRowList:
            if loopIter >= len(Listi):
                continue
            else:
                newEncode[validx]=(Listi[loopIter])
                validx+=1
    return newEncode
def number2Binary(number,MaxBinaryLen=20):
    fS = '{0:'+str(MaxBinaryLen)+'b}'
    # print((number),int(number))
    # exit(1)
    BinaryDstr = fS.format(int(number))
    # print(BinaryDstr)
    BinaryList = []
    for dig in BinaryDstr:
        if dig == ' ':
            BinaryList.append(False)
        else:
            BinaryList.append(bool(int(dig)))
    # print(BinaryList,bool(1),bool(0))
    # exit(1)
    return BinaryList
def binaryL2Decvalue(binaryL):
    acc = 0
    for i in range(len(binaryL)):
        # print(i,binaryL[len(binaryL)-i-1])
        acc*=2
        acc+=binaryL[i]
    # print(acc)
    return acc

class datainBinary(Dataset):
    def __init__(self,D):
        """
        以np数组形式传入Data:
        """
        r, c = D.shape
        self.D = D
        self.len = r
    def __len__(self):
        return self.len

    def __getitem__(self, index):

        return (self.D[index], index)

def arraydec2Z(D,BinaryDig,tok =None):
    # maxCol = np.max(D, 0)
    # BinaryDig = []
    # for maxci in maxCol:
    #     BinaryDig.append(countBinaryDig(maxci))
    r, c = D.shape
    bitcodlen = 0
    for v in BinaryDig:
        bitcodlen+=v
    ZOrderArray = np.zeros((r,bitcodlen),dtype=np.bool)

    for i in range(r):
        if i % 10000==0:
            print('\r',i,'/',r,end=' ',flush=True)
        tuple = D[i, :]
        BinaryrowList = []
        for j in range(c):
            if BinaryDig[j] == 0:
                BinaryrowList.append([0])
                continue
            BinaryrowList.append(number2Binary(tuple[j], BinaryDig[j]))
        # br = binaryRow2Zencode(BinaryrowList)
        br = fastbinaryRow2Zencode(BinaryrowList,bitcodlen,lenList=BinaryDig)

        ZOrderArray[i,:] = (br)
    return ZOrderArray



import xml.etree.ElementTree as xee


def osmProcess(filename,datalength = 10000):
    # 读取文件

    # domTree = xee.iterparse(filename,events=('end',))
    # print("start")
    # next(domTree)
    # next(domTree)
    # tag_stack = []
    # elem_stack = []

    # finalIdx = 80000000
    finalIdx = datalength
    D = np.zeros((finalIdx,2))
    idx = 0
    for event, elem in xee.iterparse(filename,events=('end',)) :
        if elem.tag == 'node':
            if idx %10000==0:
                print('\r',idx,'/',finalIdx,end=' ',flush=True)
            if idx >=finalIdx:
                break
            node = elem
            # rowList = []
            Lat =abs( int(float(node.get("lat")) * 1000))
            Lon = abs(int(float(node.get("lon")) * 1000))
            D[idx,0] =Lat
            D[idx, 1] = Lon
            elem.clear()
            # ID = abs(int(node.get("id")))
            # ver = abs(int((node.get('version'))))
            # vis = abs(int(bool(node.get('visible'))))
            # rowList.append(Lat)
            # rowList.append(Lon)
            # rowList.append(ID)
            # rowList.append(ver)
            # rowList.append(vis)
            # D.append(rowList)
            idx += 1
            # print(elem)
        # exit(1)

    # D = np.array(D)
    print(D.shape)
    r,c = D.shape
    for i in range(c):
        print('col :',i)
        print('max:',np.max(D[:,i]),"min",np.min(D[:,i]))
        # # 输出node信息
        # print('nodeID:' + ID + ', Lat:' + str(Lat) + ', Lon:' + str(Lon) + ',ver:',ver,'vis:',vis)
    np.save('./data/osmfile.npy',D)
    print('data saved')






def readosm(filename=r'./data/map/ca.osm',datalength = 10000):
    print('reading first ',datalength,'tuples')
    osmProcess(filename,datalength)

def osm2zorder():
    D = np.load('./data/osmfile.npy')
    # print(D)
    ZD = arraydec2Z((D))
    # print(ZD.shape)
    # print(ZD)
    np.save('./data/osmZD.npy', ZD)

def DMVnploader():
    csv_filename = './data/DMV.csv'
    col_offsets = [0,2,4,6,9,10,14,16,17,18,19]
    dataset = []
    rowidx=0
    strset = []
    str2intEnc = []
    for i in range(len(col_offsets)):
        a0 = set()
        strset.append(a0)

    with open(csv_filename) as csvfile:
        reader = csv.reader(csvfile)

        for line in reader:
            rowidx+=1
            if rowidx==1:
                continue
            if rowidx%100000==0:
                print('\rreading row:',rowidx,end=' ',flush=True)
                # break
            # print(len(line))
            row = []
            idx0 = 0
            for offset in col_offsets:
                row.append((line[offset]))
                strset[idx0].add(line[offset])
                idx0+=1
            dataset.append(row)
    # dataset = np.array(dataset)
    # r,c = dataset.shape
    r = len(dataset)
    c = len(col_offsets)
    intArr = np.zeros((r,c))
    for i in range(len(col_offsets)):
        m0 = {}
        idx0 = 0
        for v in strset[i]:
            m0[v] = idx0
            idx0+=1
        str2intEnc.append(m0)
    for i in range(r):
        for j in range(c):
            # print(i,j)
            # v = dataset[i][j]
            # print( str2intEnc[ j ])
            intArr[i,j] = str2intEnc[j ][ dataset[i][j] ]
    np.save('./data/DMVint.npy',intArr)
    print('done!')
    #     print(len( strset[i]))

def np2CF(npf1,npf2,outFileName1):
    r,c = npf1.shape
    r2,c2 = npf2.shape

    f = open(outFileName1,'w')
    print(str(r)+' '+str(c)+ ' '+ str(r2)+' '+ str(c2)+'\n')
    f.write(str(r)+' '+str(c)+ ' '+ str(r2)+' '+ str(c2)+'\n')
    for i in range(r):
        for j in range(c):
            f.write(str(int(npf1[i,j]))+' ')
        for j in range(c2):
            f.write(str(int(npf2[i, j])) + ' ')
        f.write('\n')
    f.close()

if __name__ == "__main__":
    DFRoot = './data/'
    # for j in range(10):
    #     i=j+10
    #     zdn = DFRoot + "ZD" + str(i) +'.npy'
    #     dn = DFRoot + "D" + str(i)+'.npy'
    #     ZD = np.load(zdn)
    #     D = np.load(dn)
    #     print(ZD.shape)
    #     print(D.shape)
    #     zdn2 = DFRoot + "ZD" + str(i) + '.txt'
    #     np2CF(ZD,D,zdn2)
    infilename = sys.argv[1]
    blockNum = int(sys.argv[2])
    #暂时还没解决排序问题
    sortFlag = sys.argv[3]

    Dname = DFRoot+infilename+'.npy'
    D = np.load(Dname)
    r,c = D.shape
    inc = int(r/blockNum)
    print('Data loaded r:',r,' c:',c,' incLen:',inc)
    # ind = np.lexsort(D.T)
    # print(len(ind))
    # exit(1)
    # D = D[ind, :]
    # print(D[:10,:])
    maxCol = np.max(D, 0)
    # print(maxCol)
    # exit(1)
    BinaryDig = []
    for maxci in maxCol:
        BinaryDig.append(countBinaryDig(maxci))
    # print(BinaryDig)
    DFRoot = './data/'
    for i in range(blockNum):
        dfn = DFRoot + 'D' + str(i)+'.npy'
        incD = D[inc*i:min(inc*(i+1),r) , : ]
        np.save(dfn,incD)
        print('Dec Data Saved shape:',incD.shape)
        print("Z Ordered DataMaking")
        ZincD = arraydec2Z(incD,BinaryDig)
        zdn = DFRoot+"ZD"+str(i)
        np.save(zdn,ZincD)
        print('Z-Data Saved shape:',ZincD.shape)
        zdn2 = DFRoot + "ZD" + str(i) + '.txt'
        np2CF(ZincD,incD,zdn2)
        print("Tuple Data Saved")