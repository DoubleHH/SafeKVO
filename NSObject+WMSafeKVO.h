//
//  NSObject+WMSafeKVO.h
//  NavigationTest
//
//  Created by DoubleHH on 2017/3/17.
//  Copyright © 2017年 com.baidu. All rights reserved.
//

/**
 Fix KVO's crashes. 
 You can [NSObject wm_addObserver:forKeyPath:context] repeatedly and
 do not worry about removeObserver. This implementation will auto
 removeObserver when observer or observered object dealloc.
 
 KVO crash in three situations:
 1. The time of calling [NSObject removeObserver:forKeyPath:context] 
    more than [NSObject addObserver:forKeyPath:context].
 2. Forget to call [NSObject removeObserver:forKeyPath:context]
    when observer or observered object dealloc and you called
    [NSObject addObserver:forKeyPath:options:context:].
 **/

#import <Foundation/Foundation.h>

@interface NSObject (WMSafeKVO)

- (void)wm_addObserver:(NSObject *)observer
            forKeyPath:(NSString *)keyPath
               options:(NSKeyValueObservingOptions)options
               context:(void *)context;

- (void)wm_removeObserver:(NSObject *)observer
               forKeyPath:(NSString *)keyPath
                  context:(void *)context;

- (void)wm_removeObserver:(NSObject *)observer
               forKeyPath:(NSString *)keyPath;

@end
