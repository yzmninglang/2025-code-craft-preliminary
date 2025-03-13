% 定义变量范围
x = linspace(0, 120, 500); % x 范围从 0 到 120
size_range = linspace(0, 10, 500); % size 范围从 0 到 10

% 创建网格
[X, Size] = meshgrid(x, size_range);

% 定义分段函数 f(x)
F = zeros(size(X)); % 初始化 F
F(X >= 0 & X <= 10) = -0.005 * X(X >= 0 & X <= 10) + 1;
F(X > 10 & X <= 105) = -0.01 * X(X > 10 & X <= 105) + 1.05;

% 定义 g(size)
G = (Size + 1) * 0.5;

% 计算 f(x) * g(size)
Z = F .* G;

% 绘制三维曲面图
figure;
surf(X, Size, Z, 'EdgeColor', 'none');
xlabel('x');
ylabel('size');
zlabel('f(x) \cdot g(size)');
title('Function Plot of f(x) \cdot g(size)');
colorbar;

% 如果需要绘制等高线图，可以使用 contour 函数
figure;
contourf(X, Size, Z, 50, 'LineColor', 'none');
xlabel('x');
ylabel('size');
title('Contour Plot of f(x) \cdot g(size)');
colorbar;