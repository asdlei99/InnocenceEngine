//
//  InnoNSApplication.m
//  InnoMain
//
//  Created by zhangdoa on 14/04/2019.
//  Copyright © 2019 InnocenceEngine. All rights reserved.
//

#import "InnoNSApplication.h"
#import "MacWindowDelegate.h"

#import "../../../Application/InnoApplication.h"
#import "MacWindowSystemBridgeImpl.h"
#import "MTRenderingServerBridgeImpl.h"

@implementation InnoNSApplication
MacWindowSystemBridgeImpl* m_macWindowSystemBridge;
MTRenderingServerBridgeImpl* m_metalRenderingServerBridge;
MacWindowDelegate* m_macWindowDelegate;
MetalDelegate* m_metalDelegate;

- (void)run
{
    [[NSNotificationCenter defaultCenter]
      postNotificationName:NSApplicationWillFinishLaunchingNotification
      object:NSApp];
    [[NSNotificationCenter defaultCenter]
     postNotificationName:NSApplicationDidFinishLaunchingNotification
     object:NSApp];
    m_macWindowDelegate = [MacWindowDelegate alloc];
    m_metalDelegate = [MetalDelegate alloc];

    m_macWindowSystemBridge = new MacWindowSystemBridgeImpl(m_macWindowDelegate, m_metalDelegate);
    m_metalRenderingServerBridge = new MTRenderingServerBridgeImpl(m_macWindowDelegate, m_metalDelegate);

    //Start the engine C++ module
    const char* l_args = "-renderer 4 -mode 0 -loglevel 1";
    if (!InnoApplication::Setup(m_macWindowSystemBridge, m_metalRenderingServerBridge, (char*)l_args))
    {
        return;
    }
    if (!InnoApplication::Initialize())
    {
        return;
    }

    InnoApplication::Run();

    InnoApplication::Terminate();

    delete m_metalRenderingServerBridge;
    delete m_macWindowSystemBridge;
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)theApplication
{
    return YES;
}

- (void)applicationWillTerminate:(NSNotification *)aNotification{
    [m_macWindowDelegate windowWillClose:aNotification];
}
@end
