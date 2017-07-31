# SafeKVO
A category to use KVO in a safe way~

# 关于KVO的那些事 -- KVO简单实现与安全用法

`KVO (Key Value Observering)` 是iOS用于监听某个对象某个变量的一种方便，使用的机制。但是，对于KVO的稳定性苹果却做得没有那么好，在以下三种情况下会无情Crash：

1. 监听者`dealloc`时，监听关系还存在。当监听值发生变化时，会给监听者的野指针发送消息，报野指针Crash。（猜测底层是保存了`unsafe_unretained`指向监听者的指针）；
2. 被监听者`dealloc`时，监听关系还存在。在监听者内存free掉后，直接会报监听者还存在监听关系而Crash；
3. 移除监听次数大于添加监听次数。报出多次移除的错误；

![](http://i.niupic.com/images/2017/07/31/e0988M.jpeg)

我们考虑到KVO带来的伤害，平时十分小心翼翼在工程内使用KVO，甚至能不用的时候就不用。不甘心将如此好用的机制沦为带刺的玫瑰，让我们为KVO塑造"黄金铠甲"。

根据崩溃类型，我们的目标也有三个：

1. 监听者`dealloc`时，自动移除监听者对其他对象的监听，No Crash；
2. 被监听者`dealloc`时，自动移除对被监听者所有的监听，No Crash；
3. 移除监听次数大于添加监听次数时，多次监听/移除，只执行一次，No Crash；

实现的源码[请点这里](https://github.com/DoubleHH/SafeKVO)。

## 1. 解决监听者（listener）`dealloc`的Crash

解决监听者`dealloc`的Crash，最直接的办法就是在监听者（listener）的`dealloc`调用`[listened removeObserver:listener forKeyPath:path]`。但此时我们会遇到3个问题：

1. 如若不采用hook `dealloc`方法添加移除逻辑（采用hook风险比较大，会对所有NSObject对象的方法hook），还有什么方法能对所有对象的`dealloc`插上一脚呢？
2. listener（监听者）不知道listened（被监听者）们是谁？
3. listener不知道listened的keypath有哪些？

第一个问题的解决方案是 关联对象。当一个对象释放时，会进行以下三个步骤：第一步，销毁对象的所有属性及实例变量，第二部，移除对象上的所有关联对象；第三步，移除所有对该对象的weak引用。

![KVO Crash三类](http://i.niupic.com/images/2017/07/27/YwsD1r.png)

关联对象的释放是在listener `dealloc`过程的第二个步骤当中，此时对象并没有完全释放。因此，我们可以给listener添加一个关联对象解决第一个问题。

根据第一个问题解决方案，第二和第三个问题也不难想出。同样，我们可用关联对象保存监听者和keypath数组解决。但我们不这么做，为了让 listener 看起来更加干净，也为了让逻辑更加清晰，可将监听者和keypath作为第一个关联对象的实例变量。而且为了不强引用监听者，监听者是`weak`保存的。另外，为了全权管理listener对listened的行为，将监听行为转给这个关联对象，关联对象收到监听消息再转递给listener。至此，我们成这个关联对象为代理者proxy。

最终，listener，listened，proxy三者关系及监听者移除监听过程如下：

![解listener Crash](http://i.niupic.com/images/2017/08/01/r0U3n0.jpeg)

#### proxy定义如下：

![proxy 定义](http://i.niupic.com/images/2017/08/01/vCKMIW.png)

我们来详细捋一遍过程：

#### 添加过程

1. 添加监听时，listener取出对listened的关联对象proxy（没有就创建，并建立新的监听关系和转发关系），并且proxy保存了listener和listened的weak属性；
2. 如果keypath在proxy保存中，说明已经监听过了，不需要再监听。<strong>此时解决了我们第三大类的移除次数大于监听次数的crash</strong>。如不在keypath中，则添加keypath到keypath容器中，并建立对于新的keypath的监听关系；

#### `dealloc`过程

listener `dealloc`，触发proxy的`dealloc`，proxy根据保存的keypath信息依次移除监听关系，至此监听关系完美解除。

第一个和第三个Crash问题解决了，接下来，我们来fix第二大问题~

## 2. 解决被监听者（listened）`dealloc`的Crash

和第一个问题类似，要解决listened的`dealloc`的Crash，同样地可以给listened添加一个关联对象用于检测listened的释放时机。如下图所示：

![](http://i.niupic.com/images/2017/08/01/2i7ILT.jpeg)

图中的D对象，也就是监听被监听者的监听者（有点绕...），Listened's Dealloc Listener简称LDL。可以看到它不仅跟listened有关系，还弱持有了proxy。为什么呢？因为对于listened来说，它自己并不知道谁监听了它，而正好proxy知道监听中的所有秘密。当listened释放时，LDL被释放，根据保存的proxy关系，就能释放对listened的所有监听关系，并且还可以移除listener的关联对象proxy。

最终，我们来看看LDL的实现代码：

![](http://i.niupic.com/images/2017/08/01/BvoUIM.png)

眼尖的同学会发现两个有趣的地方：  

1. listened 是 `__unsafe_unretained`的指针保存；
2. proxy 是 用 weak `NSHashTable` 容器保存；

为什么要用`__unsafe_unretained`保存listened，而不是`weak`？

原因是若使用`weak`，在LDL `dealloc` 的过程中，指针获取到的值已经为`nil`了（在proxy保存的listened也一样），拿不到我们要用的对象指针，那么好奇的包包又会问了这又是为什么呢？这是因为每次使用`weak`变量时，最终会调用`id objc_loadWeakRetained(id *)`方法，方法发现当前对象如果在`dealloc`过程中就会直接返回`nil`。所以，我们这里使用了`__unsafe_unretained`指针来保有对象的指针，既能一直访问到对象，又不会影响对象的引用计数。

第二个问题，`NSHashTable` 容器保存proxy，`NSHashTable`类似于数组，但它可以保存任何指针，而且可以有各种方法存储他们，比如，`retain, weak, copy...`。目的就是保存监听关系proxy列表，而且弱引用他们。这种特殊容器平时开发用的很少，想了解更多关于这些特殊容器的，[请点这里](http://nshipster.cn/nshashtable-and-nsmaptable/)

## 用法

原理讲完了，说说最终的用法，和系统源码API一样简单，提供了三个接口如下：

![](http://i.niupic.com/images/2017/08/01/OMicMn.png)

## 总结

本文从KVO三种类型的Crash进行分析，使用了代理模式做转发，用关联对象监听`dealloc`时机，使用`__unsafe_unretained`来持有不会增加引用计数但一直保有对象的指针，使用了不常见的`NSHashTable`来弱持有代理，最终实现了健壮的KVO，减少了KVO系统实现的问题导致的不愉快的使用体验，让更多人感受到使用KVO机制带来的幸福感。

[源码请点我~](https://github.com/DoubleHH/SafeKVO)。