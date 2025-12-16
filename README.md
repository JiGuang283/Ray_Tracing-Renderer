# 光线追踪渲染器

## 改进

1. 积分器改进
2. 材质改进
3. 光源改进
4. 实现外部模型加载（未实现）
5. 渲染管线优化及GUI（未实现）

## 1. 积分器优化

### 1.1 实现轮盘赌终止策略

使用俄罗斯轮盘赌的方法来提前终止光线，其思路如下：
根据其贡献值（吞吐量）随机决定是否终止光线。如果决定继续追踪，就增加这条光线的“权重（强度）”，以补偿那些被提前终止的光线所损失的能量。

#### 算法流程：
假设当前光线的累积吞吐量（Throughput，即光线携带的能量/颜色权重）为 $L_o$，我们设定一个继续追踪的概率为 $P$（$0 < P < 1$）。

1.  生成一个 $[0, 1]$ 之间的随机数 $\xi$。
2.  **判定：**
    *   **如果 $\xi > P$（概率为 $1-P$）：** 光线不幸“死亡”，终止追踪，返回结果 **0**。
    *   **如果 $\xi \leq P$（概率为 $P$）：** 光线“存活”，继续追踪下一跳。**但是**，为了守恒能量，必须将当前的光线强度除以 $P$。
        *   新强度 $L_{new} = L_o / P$

#### 最佳实践：基于吞吐量（Throughput）的动态 P

最常用的策略是根据当前光线的“贡献度”来决定 $P$。如果一条光线经过几次反弹后，打到了黑色的墙上，它的吞吐量变得很低（对最终画面贡献小），我们就应该加大它死亡的概率。

通常使用当前光线颜色的最大分量作为概率：

$$ P = \text{Clamp}(\max(Throughput.r, Throughput.g, Throughput.b), \text{min\_prob}, \text{max\_prob}) $$

或者更简单的形式（不加 Clamp，但需防止除零）：
$$ P = \max(Throughput.r, Throughput.g, Throughput.b) $$

#### 代码实现

```cpp
if (depth >= m_rr_start_depth) {
    double p_survive =
        std::max({throughput.x(), throughput.y(), throughput.z()});
    p_survive = clamp(p_survive, 0.005, 0.95);

    if (random_double() > p_survive) {
        break;
    }
    throughput /= p_survive;
}
```

#### 效果

实现轮盘赌的策略后，可以在不大幅度影响渲染结果质量加快渲染。

在标准的 cornell_box 场景中进行测试。

![原积分器](build/output/scene07_integrator0_1765799984.png)
###### 无轮盘赌终止

![轮盘赌](build/output/scene07_integrator1_1765799945.png)
###### 使用轮盘赌终止

可以看到两者图片从视觉上基本没有差别

### 1.2 实现pbr材质（BSDF采样）

为支持后续的pbr微表面模型材质，需要改进现有积分器支持BSDF采样。对于原来的光线追踪，其只定义了漫反射，金属，绝缘体三种材质以及简单的反射折射行为。而pbr材质基于微表面模型，可以更好地描述光线与材质的互动，从而更好的描述光线的反射折射方向。

### 1.3 实现直接光照采样

在传统的光线追踪中，需要光线直接打到光源上才会计算光照并贡献颜色。在光源较小的情况，光线会很难击中光源，导致画面中有较多的噪点。而直接光照采样则在光线打到物体上时，直接对光源进行采样并计算贡献值。

在相同参数设置下（samples_per_pixel = 400，MaxDepth = 50，使用轮盘赌终止策略），其结果如下：

![原积分器](build/output/scene07_integrator1_1765801791.png)
###### 原积分器结果，渲染时间：10.3s

![直接光照采样](build/output/scene21_integrator3_1765801855.png)
###### 直接光照采样，渲染时间：19.1s

可以看到尽管直接光照采样的渲染时间较长，但其噪点明显减少，且保持了光线追踪的渲染效果，如盒子的侧面有墙反射过来的颜色。

### 1.4 实现MIS

直接光源采样有一个缺点，当物体的材质较为光滑，其会镜面反射大部分的光，而光源较大时，在光源上只有一小部分会贡献高光。但普通的大型光源如面光源是随机采样，因此很难采样到贡献高光的部分。进而无法正确反射。

![alt text](build/output/image.png)

为解决这一问题，引入了多重重要性采样。

多重重要性采样在光线击中物体时即执行直接光源采样，同时也继续执行光线追踪来判断最终是否击中光源。（直接光照采样中不关注最后是否击中光源）。之后将两者的贡献进行分别加权并求和。

![alt text](build/output/image-1.png)

这样就可以解决直接光照采样在光滑表面的问题。

#### 与MIS的对比

| 图 1（直接光照采样） | 图 2（MIS） |
|-----|-----|
| ![](build/output/scene23_integrator3_1765847116.png) | ![](build/output/scene23_integrator4_1765847185.png) |
###### 图1渲染时间：1.1s，图2渲染时间：1.3s

可以看到直接光照采样中，左侧的光滑小球表面没有正确反射顶部的面光源，同时在中间小球上的像也较为模糊。

| 细节位置 | NEE (直接光照采样) | MIS (多重重要性采样) |
| :---: | :---: | :---: |
| **小球顶部** | ![NEE小球顶部](build/output/image-3.png) | ![MIS小球顶部](build/output/image-2.png) |
| **小球左侧** | ![NEE小球左侧](build/output/image-4.png) | ![MIS小球左侧](build/output/image-5.png) |

## 2. 材质优化

### 2.1 新接口

由于要实现蒙特卡洛积分，以及pbr材质，因此需要设计材质的新接口，使其在材质采样时可以获取pdf等数值。
```cpp
struct BSDFSample {
    vec3 wi; // 采样生成的入射方向
    color f; // BSDF 吞吐量, 存 f (BSDF值)，让积分器自己乘 cos 和除
             // pdf(delta材质除外)。
    double pdf;                   // 采样该方向的概率密度
    bool is_specular;             // 是否是镜面反射（Delta分布）
    bool is_transmission = false; // 区分透射
};

class material {
  public:
    virtual ~material() = default;

    // 原emitted函数（保留旧接口）
    virtual color emitted(double u, double v, const point3 &p) const {
        return color(0, 0, 0);
    }

    // 新 emitted 接口
    virtual color emitted(const hit_record &rec, const vec3 &wo) const {
        return color(0, 0, 0);
    }

    // 判断材质是否含有完美镜面（Delta 分布）成分
    virtual bool is_specular() const {
        return false;
    }

    // 采样 BSDF 生成入射方向 (Importance Sampling)
    virtual bool sample(const hit_record &rec, const vec3 &wo,
                        BSDFSample &sampled) const {
        return false;
    }

    // 给定两个方向，计算反射比率（BSDF值）
    virtual color eval(const hit_record &rec, const vec3 &wo,
                       const vec3 &wi) const {
        return color(0, 0, 0);
    }

    // 计算pdf
    virtual double pdf(const hit_record &rec, const vec3 &wo,
                       const vec3 &wi) const {
        return 0.0;
    }

    // Deprecated scatter
    virtual bool scatter(const ray &r_in, const hit_record &rec, color &albedo,
                         ray &scattered, double &pdf_val) const {
        return false;
    }

    // 保留旧接口
    virtual bool scatter(const ray &r_in, const hit_record &rec,
                         color &attenuation, ray &scattered) const {
        return false;
    }
};
```

### 2.2 PBR材质

使用 Cook-Torrance 微表面模型来描述PBR材质，以得到近似物理正确的结果。







