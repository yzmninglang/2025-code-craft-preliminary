% MATLAB 脚本: 将文件内容读取为矩阵并分割为小矩阵

% 清空工作区
clear;
clc;

% 指定文件路径
filePath = 'liuxingdu.txt'; % 确保文件路径正确

% 打开文件并读取内容
fid = fopen(filePath, 'r');
if fid == -1
    error('无法打开文件，请检查文件路径是否正确！');
end

% 读取文件内容到一个字符串
fileContent = fscanf(fid, '%c');
fclose(fid);

% 将字符串转换为数字矩阵
% 使用空格和换行符分割数据
data = str2num(fileContent);

% 确保数据是 48 行的矩阵
if size(data, 1) ~= 48
    error('输入数据的行数不是 48 行，请检查文件内容！');
end

% 初始化存储小矩阵的单元数组
subMatrices = cell(3, 1);

% 每 16 行分割为一个小矩阵
for i = 1:3
    startRow = (i-1)*16 + 1; % 当前小矩阵的起始行
    endRow = i*16;           % 当前小矩阵的结束行
    subMatrices{i} = data(startRow:endRow, :); % 提取子矩阵
end

% 显示结果
disp('分割完成的小矩阵如下：');




