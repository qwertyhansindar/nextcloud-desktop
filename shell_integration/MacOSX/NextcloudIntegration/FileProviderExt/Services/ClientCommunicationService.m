/*
 * Copyright (C) 2023 by Claudio Cambra <claudio.cambra@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#import "ClientCommunicationService.h"

#import "ClientCommunicationProtocol.h"
#import "FileProviderExt-Swift.h"

@implementation ClientCommunicationService

@synthesize serviceName = _serviceName;

- (instancetype)initWithFileProviderExtension:(FileProviderExtension *)extension
{
    self = [super init];
    if (self) {
        _serviceName = @"com.nextcloud.desktopclient.ClientCommunicationService";
        _listener = NSXPCListener.anonymousListener;
        _extension = extension;
    }
    return self;
}

- (nullable NSXPCListenerEndpoint *)makeListenerEndpointAndReturnError:(NSError * *)error
{
    return self.listener.endpoint;
}

- (BOOL)listener:(NSXPCListener *)listener
shouldAcceptNewConnection:(NSXPCConnection *)newConnection
{
    return YES;
}

- (void)configureAccountWithUser:(NSString *)user
                       serverUrl:(NSString *)serverUrl
                        password:(NSString *)password
{
    [self.extension setupDomainAccountWithUser:user
                                     serverUrl:serverUrl
                                      password:password];
}

- (void)removeAccountConfig
{
    [self.extension removeAccountConfig];
}

@end
