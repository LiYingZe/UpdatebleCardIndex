@REM 进行数据的准备，包括查询生成
@REM 格式：数据numpy 分片数量 是否排序
@REM E:\Anaconda\envs\torch\python.exe loadDataSet.py DMVint 20 T

@REM 生成查询
@REM 格式: 数据numpy 分片数量 查询数目
@REM E:\Anaconda\envs\torch\python.exe Query.py DMVint 20 100

@REM 利用Pytorch框架进行增量训练,输出增量训练后的若干个网络的state,存储与于文件夹./Model当中
E:\Anaconda\envs\torch\python.exe FancyTrain.py ./NetTrain.csv


