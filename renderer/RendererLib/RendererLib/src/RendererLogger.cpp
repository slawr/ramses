//  -------------------------------------------------------------------------
//  Copyright (C) 2013 BMW Car IT GmbH
//  -------------------------------------------------------------------------
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at https://mozilla.org/MPL/2.0/.
//  -------------------------------------------------------------------------

#include "RendererAPI/IRenderBackend.h"
#include "RendererAPI/ISurface.h"
#include "RendererAPI/IWindow.h"
#include "RendererAPI/IEmbeddedCompositor.h"
#include "SceneAPI/StreamTexture.h"
#include "RendererLib/RendererLogger.h"
#include "RendererLib/RendererLogContext.h"
#include "RendererLib/RendererSceneUpdater.h"
#include "RendererLib/SceneStateExecutor.h"
#include "RendererLib/RendererScenes.h"
#include "RendererLib/Renderer.h"
#include "RendererLib/DisplayController.h"
#include "RendererLib/SceneStateInfo.h"
#include "RendererLib/EResourceStatus.h"
#include "RendererLib/ResourceDescriptor.h"
#include "RendererLib/RendererResourceManager.h"
#include "RendererLib/LoggingDevice.h"
#include "RendererLib/TransformationLinkManager.h"
#include "RendererLib/DataReferenceLinkManager.h"
#include "RendererLib/SceneLinks.h"
#include "RendererLib/RendererCachedScene.h"
#include "RendererLib/DisplaySetup.h"
#include "RendererLib/StagingInfo.h"
#include "FrameBufferInfo.h"
#include "RenderExecutor.h"
#include "RenderExecutorLogger.h"
#include "RendererEventCollector.h"
#include "Utils/LogMacros.h"
#include "Common/Cpp11Macros.h"

namespace ramses_internal
{
    void RendererLogger::StartSection(const String& name, RendererLogContext& context)
    {
        context << RendererLogContext::NewLine << "+++ " << name << " +++" << RendererLogContext::NewLine << RendererLogContext::NewLine;
    }

    void RendererLogger::EndSection(const String& name, RendererLogContext& context)
    {
        context << RendererLogContext::NewLine << "--- " << name << " ---" << RendererLogContext::NewLine << RendererLogContext::NewLine;
    }

    void RendererLogger::Log(const LogContext& logContext, const RendererLogContext& context)
    {
        if (context.isLogLevelFlagEnabled(ERendererLogLevelFlag_Details) || context.isLogLevelFlagEnabled(ERendererLogLevelFlag_Info))
        {
            LOG_INFO(logContext, context.getStream().c_str());
        }
        else if (context.isLogLevelFlagEnabled(ERendererLogLevelFlag_Warn))
        {
            LOG_WARN(logContext, context.getStream().c_str());
        }
        else if (context.isLogLevelFlagEnabled(ERendererLogLevelFlag_Error))
        {
            LOG_ERROR(logContext, context.getStream().c_str());
        }
    }

    void RendererLogger::LogTopic(const RendererSceneUpdater& updater, ERendererLogTopic topic, Bool verbose, NodeHandle nodeHandleFilter)
    {
        RendererLogContext context(verbose ? ERendererLogLevelFlag_Details : ERendererLogLevelFlag_Info);
        context.setNodeHandleFilter(nodeHandleFilter);
        switch (topic)
        {
        case ERendererLogTopic_Displays:
            LogDisplays(updater, context);
            break;
        case ERendererLogTopic_SceneStates:
            LogSceneStates(updater, context);
            break;
        case ERendererLogTopic_StreamTextures:
            LogStreamTextures(updater, context);
            break;
        case ERendererLogTopic_Resources:
            LogResources(updater, context);
            break;
        case ERendererLogTopic_MissingResources:
            LogMissingResources(updater, context);
            break;
        case ERendererLogTopic_RenderQueue:
            LogRenderQueue(updater, context);
            break;
        case ERendererLogTopic_Links:
            LogLinks(updater.m_rendererScenes, context);
            break;
        case ERendererLogTopic_EmbeddedCompositor:
            LogEmbeddedCompositor(updater, context);
            break;
        case ERendererLogTopic_EventQueue:
            LogEventQueue(updater, context);
            break;
        case ERendererLogTopic_All:
            LogDisplays(updater, context);
            LogSceneStates(updater, context);
            LogStreamTextures(updater, context);
            LogResources(updater, context);
            LogRenderQueue(updater, context);
            LogLinks(updater.m_rendererScenes, context);
            LogEmbeddedCompositor(updater, context);
            LogEventQueue(updater, context);
            break;
        case ERendererLogTopic_PeriodicLog:
            LogPeriodicInfo(updater);
            break;
        default:
            context << "Log topic " << EnumToString(topic) << " not found!" << RendererLogContext::NewLine;
            break;
        }
        if (topic != ERendererLogTopic_PeriodicLog)
        {
            Log(CONTEXT_RENDERER, context);
        }
    }

    void RendererLogger::LogSceneStates(const RendererSceneUpdater& updater, RendererLogContext& context)
    {
        StartSection("RENDERER SCENE STATES", context);
        SceneIdVector knownSceneIds;
        updater.m_sceneStateExecutor.m_scenesStateInfo.getKnownSceneIds(knownSceneIds);
        context << knownSceneIds.size() << " known scene(s)" << RendererLogContext::NewLine << RendererLogContext::NewLine;
        context.indent();

        for(const auto sceneId : knownSceneIds)
        {
            const ESceneState sceneState = updater.m_sceneStateExecutor.getSceneState(sceneId);
            const Guid clientGuid = updater.m_sceneStateExecutor.m_scenesStateInfo.getSceneClientGuid(sceneId);

            String sceneName("N/A");
            if (updater.m_rendererScenes.hasScene(sceneId))
            {
                RendererCachedScene& rendererScene = updater.m_rendererScenes.getScene(sceneId);
                sceneName = rendererScene.getName();
            }
            DisplayHandle displayHandle = DisplayHandle::Invalid();
            if (sceneState == ESceneState_MappingAndUploading || sceneState == ESceneState_Mapped || sceneState == ESceneState_Rendered)
            {
                displayHandle = updater.m_renderer.getDisplaySceneIsMappedTo(sceneId);
            }

            context << "Scene [id: " << sceneId.getValue() << "]" << RendererLogContext::NewLine;
            context.indent();
            {
                context << "State:    " << EnumToString(sceneState) << RendererLogContext::NewLine;
                if (context.isLogLevelFlagEnabled(ERendererLogLevelFlag_Details))
                {
                    context << "Name:     \"" << sceneName << "\"" << RendererLogContext::NewLine;
                    if (displayHandle.isValid())
                    {
                        context << "Display:  " << displayHandle.asMemoryHandle() << RendererLogContext::NewLine;
                        context << "Assigned to buffer (device handle): " << updater.m_renderer.getBufferSceneIsMappedTo(sceneId) << RendererLogContext::NewLine;
                    }
                    else
                    {
                        context << "Display:  " << "N/A" << RendererLogContext::NewLine;
                    }
                    context << "Client:   " << "\"" << clientGuid << "\"" << RendererLogContext::NewLine;
                }

                if (updater.hasPendingFlushes(sceneId))
                {
                    const auto& stagingInfo = updater.m_rendererScenes.getStagingInfo(sceneId);
                    const auto& pendingFlushes = stagingInfo.pendingFlushes;
                    context << "Pending flushes: " << pendingFlushes.size() << RendererLogContext::NewLine;
                    context.indent();
                    for (const auto& flushInfo : pendingFlushes)
                    {
                        context << "Flush " << flushInfo.flushIndex << " - " << (flushInfo.isSynchronous ? "synchronous " : "") << RendererLogContext::NewLine;
                        context << "[ sceneActions: " << flushInfo.sceneActions.numberOfActions() << " (" << flushInfo.sceneActions.collectionData().size() << " bytes) ]" << RendererLogContext::NewLine;
                        context << "[ " << stagingInfo.sizeInformation.asString().c_str() << " ]" << RendererLogContext::NewLine;
                        context << "[ needed resources (squashed with previous pending flushes): " << flushInfo.clientResourcesNeeded.size() << " : [";
                        for (const auto& res : flushInfo.clientResourcesNeeded)
                        {
                            context << " " << StringUtils::HexFromResourceContentHash(res);
                        }
                        context << "] ]" << RendererLogContext::NewLine;
                    }
                    context.unindent();
                }
            }
            context << RendererLogContext::NewLine;
            context.unindent();
        }
        context.unindent();
        EndSection("RENDERER SCENE STATES", context);
    }

    void RendererLogger::LogDisplays(const RendererSceneUpdater& updater, RendererLogContext& context)
    {
        StartSection("RENDERER DISPLAYS", context);
        context << updater.m_displayResourceManagers.count() << " known Display(s)" << RendererLogContext::NewLine << RendererLogContext::NewLine;

        context.indent();
        for(const auto& displayIt :updater.m_renderer.m_displays)
        {
            const DisplayHandle displayHandle = displayIt.first;
            const auto& displayController = *displayIt.second.displayController;
            const UInt32 height = displayController.getDisplayHeight();
            const UInt32 width = displayController.getDisplayWidth();
            const WaylandIviSurfaceId surfaceId = displayController.getRenderBackend().getSurface().getWindow().getWaylandIviSurfaceID();

            context << "Display [id: " << displayHandle << "]" << RendererLogContext::NewLine;

            context.indent();
            {
                context << "Width:     " << width << RendererLogContext::NewLine;
                context << "Height:    " << height << RendererLogContext::NewLine;
                context << "IviSurfaceId: " << surfaceId.getValue() << RendererLogContext::NewLine;
                if (context.isLogLevelFlagEnabled(ERendererLogLevelFlag_Details))
                {
                    context << "Frame Buffer:" << RendererLogContext::NewLine;
                    context.indent();
                    LogDisplayBuffer(updater, displayIt.second.frameBufferDeviceHandle, displayIt.second.buffersSetup.getDisplayBuffer(displayIt.second.frameBufferDeviceHandle), context);
                    context.unindent();

                    context << "Offscreen Buffers:" << RendererLogContext::NewLine;
                    context.indent();
                    for (const auto& dispBuffer : displayIt.second.buffersSetup.getDisplayBuffers())
                    {
                        if (dispBuffer.second.isOffscreenBuffer)
                            LogDisplayBuffer(updater, dispBuffer.first, dispBuffer.second, context);
                    }
                    context.unindent();
                }
            }
            context << RendererLogContext::NewLine;
            context.unindent();
        }
        context.unindent();
        EndSection("RENDERER DISPLAYS", context);
    }

    void RendererLogger::LogDisplayBuffer(const RendererSceneUpdater& updater, DeviceResourceHandle bufferHandle, const DisplayBufferInfo& dispBufferInfo, RendererLogContext& context)
    {
        context << "Display Buffer device handle: " << bufferHandle.asMemoryHandle() << (dispBufferInfo.isInterruptible ? " [interruptible]" : "") << RendererLogContext::NewLine;
        const auto& vp = dispBufferInfo.viewport;
        context << "[ Width x Height: " << vp.width << " x " << vp.height << "  PosX x PosY: " << vp.posX << " x " << vp.posY << " ]" << RendererLogContext::NewLine;
        context << "Scenes in Rendering Order:" << RendererLogContext::NewLine;
        context.indent();
        for (const auto& sceneInfo : dispBufferInfo.mappedScenes)
        {
            const String& sceneName = updater.m_rendererScenes.getScene(sceneInfo.sceneId).getName();
            const Bool shown = ( updater.m_sceneStateExecutor.getSceneState(sceneInfo.sceneId) == ESceneState_Rendered );
            context << "[ SceneId: " << sceneInfo.sceneId.getValue() << "; order: " << sceneInfo.globalSceneOrder << "; shown: " << shown << "; name: \"" << sceneName << "\" ]" << RendererLogContext::NewLine;
        }
        context.unindent();
    }

    void RendererLogger::LogResources(const RendererSceneUpdater& updater, RendererLogContext& context)
    {
        StartSection("RENDERER RESOURCES", context);
        context << "Resources for " << updater.m_displayResourceManagers.count() << " Display(s)" << RendererLogContext::NewLine << RendererLogContext::NewLine;

        context.indent();
        for(const auto& managerIt : updater.m_displayResourceManagers)
        {
            const RendererResourceManager* resourceManager = static_cast<const RendererResourceManager*>(managerIt.value);
            context << "Display [id: " << managerIt.key << "]" << RendererLogContext::NewLine;
            context.indent();
            context << resourceManager->m_clientResourceRegistry.getAllResourceDescriptors().count() << " Resources" << RendererLogContext::NewLine << RendererLogContext::NewLine;
            context << "Status:" << RendererLogContext::NewLine;
            context.indent();

            // Infos by resource status
            for (UInt32 i = 0; i < EResourceStatus_NUMBER_OF_ELEMENTS; i++)
            {
                const EResourceStatus status = static_cast<EResourceStatus>(i);

                UInt32 count = 0;
                for(const auto& descriptorIt : resourceManager->m_clientResourceRegistry.getAllResourceDescriptors())
                {
                    const ResourceDescriptor& resourceDescriptor = descriptorIt.value;
                    if (resourceDescriptor.status == status)
                    {
                        ++count;
                    }
                }
                context << count << " " << EnumToString(status) << RendererLogContext::NewLine;
            }
            context.unindent();

            if (context.isLogLevelFlagEnabled(ERendererLogLevelFlag_Details))
            {
                context << RendererLogContext::NewLine << "Details: " << RendererLogContext::NewLine;
                context.indent();
                //Infos by resource type
                for (UInt32 i = 0; i < EResourceType_NUMBER_OF_ELEMENTS; i++)
                {
                    const EResourceType type = static_cast<EResourceType>(i);

                    UInt32 count = 0;
                    context << EnumToString(type) << RendererLogContext::NewLine;

                    context.indent();
                    for(const auto& resourceIt : resourceManager->m_clientResourceRegistry.getAllResourceDescriptors())
                    {
                        const ResourceDescriptor& resourceDescriptor = resourceIt.value;
                        if (resourceDescriptor.type == type)
                        {
                            context << "[";
                            context << "handle: " << resourceDescriptor.deviceHandle << "; ";
                            context << "hash: " << resourceDescriptor.hash << "; ";
                            context << "scene usage: (";
                            for(const auto sceneId : resourceDescriptor.sceneUsage)
                            {
                                context << sceneId.getValue() << ", ";
                            }
                            context << "); ";
                            context << "status: " << EnumToString(resourceDescriptor.status);
                            context << "]" << RendererLogContext::NewLine;
                            count++;
                        }
                    }
                    context << count << " resources of type " << EnumToString(type) << RendererLogContext::NewLine;
                    context.unindent();
                }
                context.unindent();
            }
            context.unindent();
        }
        context.unindent();

        EndSection("RENDERER RESOURCES", context);
    }

    void RendererLogger::LogMissingResources(const RendererSceneUpdater& updater, RendererLogContext& context)
    {
        StartSection("RENDERER MISSING RESOURCES", context);
        HashMap<SceneId, std::pair<size_t, ResourceContentHashVector>> missingResourcesPerScene;

        for (const auto& managerIt : updater.m_displayResourceManagers)
        {
            const RendererResourceManager* resourceManager = static_cast<const RendererResourceManager*>(managerIt.value);
            const auto& resourceDescriptors = resourceManager->m_clientResourceRegistry.getAllResourceDescriptors();
            for (const auto& descriptorIt : resourceDescriptors)
            {
                const ResourceDescriptor& rd = descriptorIt.value;
                for (const auto sceneId : rd.sceneUsage)
                {
                    auto& resCounter = missingResourcesPerScene[sceneId];
                    resCounter.first++;
                    if (rd.status != EResourceStatus_Uploaded)
                        resCounter.second.push_back(descriptorIt.key);
                }
            }
        }

        for (const auto& missingResIt : missingResourcesPerScene)
        {
            const SceneId sceneId = missingResIt.key;
            const DisplayHandle display = updater.m_renderer.getDisplaySceneIsMappedTo(sceneId);
            if (display.isValid())
            {
                const ResourceContentHashVector& missingResources = missingResIt.value.second;
                context << "Scene " << sceneId << ": " << missingResources.size() << " resources not uploaded (" << missingResIt.value.first << " total)" << RendererLogContext::NewLine;
                context.indent();
                if (!missingResources.empty())
                {
                    const RendererResourceManager* resourceManager = static_cast<const RendererResourceManager*>(*updater.m_displayResourceManagers.get(display));
                    for (const auto& hash : missingResources)
                    {
                        const auto& resourceDescriptor = resourceManager->m_clientResourceRegistry.getResourceDescriptor(hash);
                        context << "[";
                        context << "hash: " << resourceDescriptor.hash << "; ";
                        context << "handle: " << resourceDescriptor.deviceHandle << "; ";
                        context << "scene usage:";
                        for (const auto sId : resourceDescriptor.sceneUsage)
                        {
                            context << " " << sId.getValue();
                        }
                        context << "; ";
                        context << "status: " << EnumToString(resourceDescriptor.status) << "; ";
                        context << EnumToString(resourceDescriptor.type);
                        context << "]" << RendererLogContext::NewLine;
                    }
                }
                context.unindent();
            }
        }

        auto resourcesToString = [](const ResourceContentHashVector& resources)
        {
            StringOutputStream str;
            str << "[";
            for (const auto& res : resources)
                str << " " << StringUtils::HexFromResourceContentHash(res);
            str << " ]";
            return str.release();
        };

        for (const auto& managerIt : updater.m_displayResourceManagers)
        {
            context << "Display " << managerIt.key << " resource cached lists:" << RendererLogContext::NewLine;
            context.indent();
            const RendererResourceManager& resourceManager = static_cast<const RendererResourceManager&>(*managerIt.value);
            const RendererClientResourceRegistry& resRegistry = resourceManager.m_clientResourceRegistry;
            context << "ToRequest = " << resourcesToString(resRegistry.getAllRegisteredResources()) << RendererLogContext::NewLine;
            context << "Requested = " << resourcesToString(resRegistry.getAllRequestedResources()) << RendererLogContext::NewLine;
            context << "ToUpload  = " << resourcesToString(resRegistry.getAllProvidedResources()) << RendererLogContext::NewLine;
            context << "ToUnload  = " << resourcesToString(resRegistry.getAllResourcesNotInUseByScenes()) << RendererLogContext::NewLine;
            context << "ToCancel  = " << resourcesToString(resRegistry.getAllResourcesNotInUseByScenesAndNotUploaded()) << RendererLogContext::NewLine;
            context.unindent();
        }

        for (const auto& sceneIt : updater.m_rendererScenes)
        {
            context << "Scene " << sceneIt.key << RendererLogContext::NewLine;
            context.indent();
            for (const auto& pendingFlush : sceneIt.value.stagingInfo->pendingFlushes)
            {
                context << "Pending flush " << pendingFlush.flushIndex << " consolidated lists:" << RendererLogContext::NewLine;
                context.indent();
                context << "Needed = " << resourcesToString(pendingFlush.clientResourcesNeeded) << RendererLogContext::NewLine;
                context << "Unneeded = " << resourcesToString(pendingFlush.clientResourcesUnneeded) << RendererLogContext::NewLine;
                context << "PendingUnneeded = " << resourcesToString(pendingFlush.clientResourcesPendingUnneeded) << RendererLogContext::NewLine;
                context.unindent();
            }
            context.unindent();
        }
        EndSection("RENDERER MISSING RESOURCES", context);
    }

    void RendererLogger::LogRenderQueue(const RendererSceneUpdater& updater, RendererLogContext& context)
    {
        StartSection("RENDERER QUEUE", context);

        const Renderer& renderer = updater.m_renderer;
        context << "Displays that were rendered onto last frame (can vary frame to frame!)" << RendererLogContext::NewLine;

        context.indent();
        for (const auto displayHandle : renderer.m_tempDisplaysToRender)
        {
            const auto& displayInfo = renderer.m_displays.find(displayHandle)->second;
            const auto& displayBuffers = displayInfo.buffersSetup.getDisplayBuffers();

            context << RendererLogContext::NewLine;
            context << "Display [id: " << displayHandle.asMemoryHandle() << "]" << RendererLogContext::NewLine;
            context.indent();

            // 1. OBs
            context << RendererLogContext::NewLine;
            context << "<<< Skipping of frames is NOT considered! Logging here as if buffer was fully re-rendered! >>>" << RendererLogContext::NewLine;
            context << "Offscreen Buffers:" << RendererLogContext::NewLine;

            for (const auto& bufferInfo : displayBuffers)
            {
                if (bufferInfo.second.isOffscreenBuffer && !bufferInfo.second.isInterruptible)
                {
                    context.indent();
                    LogRenderQueueOfScenesRenderedToBuffer(updater, context, displayHandle, bufferInfo.first);
                    context.unindent();
                }
            }

            // 2. FB
            context << RendererLogContext::NewLine;
            context << "<<< Skipping of frames is NOT considered! Logging here as if buffer was fully re-rendered! >>>" << RendererLogContext::NewLine;
            context << "Framebuffer:" << RendererLogContext::NewLine;

            context.indent();
            LogRenderQueueOfScenesRenderedToBuffer(updater, context, displayHandle, displayInfo.frameBufferDeviceHandle);
            context.unindent();

            // 3. Interruptible OBs
            context << RendererLogContext::NewLine;
            context << "<<< Interrupted rendering and skipping of frames are NOT considered! Logging here as if buffer and scenes were fully re-rendered! >>>" << RendererLogContext::NewLine;
            context << "Interruptible Offscreen Buffers:" << RendererLogContext::NewLine;

            for (const auto& bufferInfo : displayBuffers)
            {
                if (bufferInfo.second.isInterruptible)
                {
                    context.indent();
                    LogRenderQueueOfScenesRenderedToBuffer(updater, context, displayHandle, bufferInfo.first);
                    context.unindent();
                }
            }

            context.unindent();
        }
        context.unindent();
        EndSection("RENDERER QUEUE", context);
    }

    void RendererLogger::LogRenderQueueOfScenesRenderedToBuffer(const RendererSceneUpdater& updater, RendererLogContext& context, DisplayHandle display, DeviceResourceHandle buffer)
    {
        context << "Display Buffer device handle: " << buffer << RendererLogContext::NewLine;

        const auto& displayInfo = updater.m_renderer.m_displays.find(display)->second;
        const DisplayController& displayController = static_cast<const DisplayController&>(*displayInfo.displayController);
        const auto& displayBuffers = displayInfo.buffersSetup.getDisplayBuffers();
        const DisplayBufferInfo& bufferInfo = displayBuffers.find(buffer)->second;

        const MappedScenes& mappedScenes = bufferInfo.mappedScenes;
        for (const auto& sceneInfo : mappedScenes)
        {
            if (sceneInfo.shown)
            {
                const RendererCachedScene& renderScene = updater.m_renderer.m_rendererScenes.getScene(sceneInfo.sceneId);
                LoggingDevice logDevice(displayController.getRenderBackend().getDevice(), context);
                const Viewport vp(0, 0, displayController.getDisplayWidth(), displayController.getDisplayHeight());
                const FrameBufferInfo fbInfo(displayController.m_postProcessing->getScenesRenderTarget(), displayController.m_projectionParams, vp);
                RenderExecutorLogger executor(logDevice, fbInfo, context);
                executor.logScene(renderScene, displayController.getViewMatrix());
            }
        }
    }

    void RendererLogger::LogStreamTextures(const RendererSceneUpdater& updater, RendererLogContext& context)
    {
        StartSection("RENDERER STREAM TEXTURES", context);
        SceneIdVector knownSceneIds;
        updater.m_sceneStateExecutor.m_scenesStateInfo.getKnownSceneIds(knownSceneIds);

        for(const auto sceneId : knownSceneIds)
        {
            if (updater.m_rendererScenes.hasScene(sceneId))
            {
                RendererCachedScene& scene = updater.m_rendererScenes.getScene(sceneId);

                context << "Scene [id: " << scene.getSceneId().getValue() << "; Name: " << scene.getName() << "]" << RendererLogContext::NewLine << RendererLogContext::NewLine;
                context.indent();

                const UInt32 numberOfStreamTextures = scene.getStreamTextureCount();
                for (StreamTextureHandle streamTextureHandle(0); streamTextureHandle < numberOfStreamTextures; ++streamTextureHandle )
                {
                    if(scene.isStreamTextureAllocated(streamTextureHandle))
                    {
                        const StreamTexture& streamTex = scene.getStreamTexture(streamTextureHandle);
                        const StreamTextureSourceId streamSource(streamTex.source);
                        Bool contentAvailable = false;
                        DeviceResourceHandle streamDeviceHandle;
                        DeviceResourceHandle fallbackDeviceHandle;

                        for(const auto& displayResourceManager : updater.m_displayResourceManagers)
                        {
                            const DisplayHandle displayHandle = displayResourceManager.key;
                            const IRendererResourceManager& resourceManager = *displayResourceManager.value;
                            assert(updater.m_renderer.hasDisplayController(displayHandle));
                            const IEmbeddedCompositingManager& embeddedCompositingManager = updater.m_renderer.getDisplayController(displayHandle).getEmbeddedCompositingManager();
                            const IEmbeddedCompositor& embeddedCompositor                 = updater.m_renderer.getDisplayController(displayHandle).getRenderBackend().getEmbeddedCompositor();

                            contentAvailable |= embeddedCompositor.isContentAvailableForStreamTexture(streamSource);
                            DeviceResourceHandle tempResourceHandle = embeddedCompositingManager.getCompositedTextureDeviceHandleForStreamTexture(streamSource);
                            if(tempResourceHandle.isValid())
                            {
                                streamDeviceHandle = tempResourceHandle;
                            }

                            tempResourceHandle = resourceManager.getClientResourceDeviceHandle(streamTex.fallbackTexture);
                            if(tempResourceHandle.isValid())
                            {
                                fallbackDeviceHandle = tempResourceHandle;
                            }
                        }

                        Vector<TextureSamplerHandle> samplerList;
                        for (TextureSamplerHandle i(0); i < scene.getTextureSamplerCount(); i++)
                        {
                            if (scene.isTextureSamplerAllocated(i) && scene.getTextureSampler(i).contentType == TextureSampler::ContentType::StreamTexture && scene.getTextureSampler(i).contentHandle == streamTextureHandle.asMemoryHandle())
                            {
                                samplerList.push_back(i);
                            }
                        }

                        context << "StreamTexture [handle: " << streamTextureHandle << "]" << RendererLogContext::NewLine;
                        context.indent();
                        {
                            context << "Source:               " << streamSource.getValue() << RendererLogContext::NewLine;
                            if(streamDeviceHandle.isValid())
                            {
                                context << "Device handle:        " << streamDeviceHandle.asMemoryHandle() << RendererLogContext::NewLine;
                            }
                            else
                            {
                                context << "Device handle:        invalid" << RendererLogContext::NewLine;
                            }
                            context << "Content is available: " << (contentAvailable ? "yes" : "no") << RendererLogContext::NewLine;
                            context << RendererLogContext::NewLine;

                            // Fallback section
                            context << "FallbackTexture" << RendererLogContext::NewLine;
                            context.indent();
                            context << "Fallback is forced: " << (streamTex.forceFallbackTexture ? "yes" : "no") << RendererLogContext::NewLine;
                            if(fallbackDeviceHandle.isValid())
                            {
                                context << "Device handle:      " << fallbackDeviceHandle.asMemoryHandle() << RendererLogContext::NewLine;
                            }
                            else
                            {
                                context << "Device handle:      invalid" << RendererLogContext::NewLine;
                            }
                            context << "Hash:               " << streamTex.fallbackTexture << RendererLogContext::NewLine;
                            context.unindent();
                        }
                        context << RendererLogContext::NewLine;

                        // Sampler section
                        if(samplerList.empty())
                        {
                            context << "No sampler associated with this stream texture" << RendererLogContext::NewLine;
                        }
                        else
                        {
                            context << "Sampler using this stream texture" << RendererLogContext::NewLine;
                            context.indent();
                            for(const auto& sampler: samplerList)
                            {
                                DeviceResourceHandle samplerDeviceHandle = scene.getCachedHandlesForTextureSamplers()[sampler.asMemoryHandle()];
                                context << RendererLogContext::NewLine;
                                context << "Sampler [handle: " << sampler.asMemoryHandle() << "]" << RendererLogContext::NewLine;
                                context.indent();
                                if(samplerDeviceHandle.isValid())
                                {
                                    context << "Device handle:    " << samplerDeviceHandle.asMemoryHandle() << RendererLogContext::NewLine;
                                }
                                else
                                {
                                    context << "Device handle:    invalid" << RendererLogContext::NewLine;
                                }
                                context.unindent();
                            }
                            context.unindent();
                        }
                        context << RendererLogContext::NewLine;

                        context.unindent();
                    }
                }
                context.unindent();
            }
        }
        EndSection("RENDERER STREAM TEXTURES", context);
    }

    void RendererLogger::LogLinks(const RendererScenes& scenes, RendererLogContext& context)
    {
        StartSection("RENDERER LINKS", context);
        context << scenes.count() << " known Scene(s)" << RendererLogContext::NewLine << RendererLogContext::NewLine;

        context.indent();

        // Iterate through all scenes and list all consumers & providers with type and connections!!
        for(const auto& sceneIt : scenes)
        {
            const RendererCachedScene& scene = *sceneIt.value.scene;
            const SceneId sceneId = sceneIt.key;
            const UInt32 sceneSlotCount = scene.getDataSlotCount();

            context << "Scene [id: " << sceneId.getValue() << "]" << RendererLogContext::NewLine << RendererLogContext::NewLine;
            context.indent();
            context << "Provider(s)" << RendererLogContext::NewLine;
            context.indent();

            UInt32 slotCount = 0u;
            for (DataSlotHandle slotHandle(0u); slotHandle < sceneSlotCount; slotHandle++)
            {
                if (!scene.isDataSlotAllocated(slotHandle))
                {
                    continue;
                }

                const EDataSlotType slotType = scene.getDataSlot(slotHandle).type;
                if (slotType == EDataSlotType_TransformationProvider || slotType == EDataSlotType_DataProvider || slotType == EDataSlotType_TextureProvider)
                {
                    LogProvider(scenes, context, scene, slotHandle);
                    slotCount++;
                }
            }
            context.unindent();
            context << slotCount << " provider(s) found" << RendererLogContext::NewLine << RendererLogContext::NewLine;

            context << "Consumer(s)" << RendererLogContext::NewLine;
            context.indent();
            slotCount = 0u;
            for (DataSlotHandle slotHandle(0u); slotHandle < sceneSlotCount; slotHandle++)
            {
                if (!scene.isDataSlotAllocated(slotHandle))
                {
                    continue;
                }

                const EDataSlotType slotType = scene.getDataSlot(slotHandle).type;
                if (slotType == EDataSlotType_TransformationConsumer || slotType == EDataSlotType_DataConsumer || slotType == EDataSlotType_TextureConsumer)
                {
                    LogConsumer(scenes, context, scene, slotHandle);
                    slotCount++;
                }
            }
            context.unindent();
            context << slotCount << " consumer(s) found" << RendererLogContext::NewLine << RendererLogContext::NewLine;
            context.unindent();
        }
        context.unindent();

        EndSection("RENDERER LINKS", context);
    }

    void RendererLogger::LogProvider(const RendererScenes& scenes, RendererLogContext& context, const RendererCachedScene& scene, const DataSlotHandle slotHandle, bool asLinked)
    {
        const SceneId providerSceneId = scene.getSceneId();
        const EDataSlotType slotType = scene.getDataSlot(slotHandle).type;

        const SceneLinks& sceneLinks = GetSceneLinks(scenes.getSceneLinksManager(), slotType);
        const Bool isLinked = sceneLinks.hasLinkedConsumers(providerSceneId, slotHandle);
        const DataSlotId providerId = scene.getDataSlot(slotHandle).id;

        context << "[";
        if (asLinked)
        {
            context << "sceneId: " << scene.getSceneId().getValue() << "; ";
        }
        context << "providerId: " << providerId.getValue() << "; ";
        LogSpecialSlotInfo(context, scene, slotHandle);
        context << "linked: " << (isLinked ? "yes" : "no");
        context << "]" << RendererLogContext::NewLine;

        if (!asLinked && isLinked && context.isLogLevelFlagEnabled(ERendererLogLevelFlag_Details))
        {
            context.indent();
            context << "linked to" << RendererLogContext::NewLine;
            context.indent();

            SceneLinkVector consumerLinks;
            sceneLinks.getLinkedConsumers(scene.getSceneId(), slotHandle, consumerLinks);
            for(const auto& consumerLink : consumerLinks)
            {
                LogConsumer(scenes, context, scenes.getScene(consumerLink.consumerSceneId), consumerLink.consumerSlot, true);
            }

            context.unindent();
            context.unindent();
        }
    }

    void RendererLogger::LogConsumer(const RendererScenes& scenes, RendererLogContext& context, const RendererCachedScene& scene, const DataSlotHandle slotHandle, bool asLinked)
    {
        const SceneId consumerSceneId = scene.getSceneId();
        const EDataSlotType slotType = scene.getDataSlot(slotHandle).type;
        const SceneLinks& sceneLinks = GetSceneLinks(scenes.getSceneLinksManager(), slotType);
        const OffscreenBufferHandle linkedOB = GetOffscreenBufferLinkedToConsumer(scenes, consumerSceneId, slotHandle);
        const Bool isLinked = sceneLinks.hasLinkedProvider(consumerSceneId, slotHandle) || linkedOB.isValid();
        const DataSlotId consumerId = scene.getDataSlot(slotHandle).id;

        context << "[";
        if (asLinked)
        {
            context << "sceneId: " << consumerSceneId.getValue() << "; ";
        }
        context << "consumerId: " << consumerId.getValue() << "; ";
        LogSpecialSlotInfo(context, scene, slotHandle);
        context << "linked: " << (isLinked ? "yes" : "no");
        context << "]" << RendererLogContext::NewLine;

        if (!asLinked && isLinked && context.isLogLevelFlagEnabled(ERendererLogLevelFlag_Details))
        {
            context.indent();
            context << "linked to" << RendererLogContext::NewLine;
            context.indent();

            if (linkedOB.isValid())
            {
                context << "[ offscreen buffer id " << linkedOB.asMemoryHandle() << " ]" << RendererLogContext::NewLine;
            }
            else
            {
                const SceneLink& sceneLink = sceneLinks.getLinkedProvider(consumerSceneId, slotHandle);
                LogProvider(scenes, context, scenes.getScene(sceneLink.providerSceneId), sceneLink.providerSlot, true);
            }

            context.unindent();
            context.unindent();
        }
    }

    void RendererLogger::LogSpecialSlotInfo(RendererLogContext& context, const RendererCachedScene& scene, const DataSlotHandle slotHandle)
    {
        const DataSlot& dataSlot = scene.getDataSlot(slotHandle);
        switch (dataSlot.type)
        {
        case EDataSlotType_TransformationProvider:
        case EDataSlotType_TransformationConsumer:
        {
            context << "type: transformation; ";
            context << "node: \"" << dataSlot.attachedNode.asMemoryHandle() << "\"; ";
        }
        break;
        case EDataSlotType_DataProvider:
        case EDataSlotType_DataConsumer:
        {
            const EDataType datatype = GetDataTypeForSlot(scene, slotHandle);
            context << "type: data; ";
            context << "datatype: " << EnumToString(datatype) << "; ";
        }
        break;
        case EDataSlotType_TextureProvider:
        {
            const ResourceContentHash& hash = dataSlot.attachedTexture;
            context << "type: texture; ";
            context << "hash: " << hash << "; ";
        }
        break;
        case EDataSlotType_TextureConsumer:
        {
            const TextureSamplerHandle& handle = dataSlot.attachedTextureSampler;
            context << "type: texture; ";
            context << "handle: " << handle << "; ";
        }
        break;
        default:
            assert(false);
        }
    }

    const SceneLinks& RendererLogger::GetSceneLinks(const SceneLinksManager& linkManager, const EDataSlotType type)
    {
        switch (type)
        {
        case EDataSlotType_TransformationProvider:
        case EDataSlotType_TransformationConsumer:
            return linkManager.getTransformationLinkManager().getSceneLinks();
        case EDataSlotType_DataProvider:
        case EDataSlotType_DataConsumer:
            return linkManager.getDataReferenceLinkManager().getSceneLinks();
        case EDataSlotType_TextureProvider:
        case EDataSlotType_TextureConsumer:
            return linkManager.getTextureLinkManager().getSceneLinks();
        default:
            assert(false);
            static SceneLinks links;
            return links;
        }
    }

    EDataType RendererLogger::GetDataTypeForSlot(const RendererCachedScene& scene, const DataSlotHandle slotHandle)
    {
        const DataInstanceHandle dataReference = scene.getDataSlot(slotHandle).attachedDataReference;
        assert(dataReference.isValid());

        const DataLayoutHandle dataLayoutHandle = scene.getLayoutOfDataInstance(dataReference);
        assert(dataLayoutHandle.isValid());

        return scene.getDataLayout(dataLayoutHandle).getField(DataFieldHandle(0u)).dataType;
    }

    OffscreenBufferHandle RendererLogger::GetOffscreenBufferLinkedToConsumer(const RendererScenes& scenes, SceneId consumerScene, DataSlotHandle consumerSlot)
    {
        assert(scenes.hasScene(consumerScene));
        const IScene& scene = scenes.getScene(consumerScene);
        const TextureSamplerHandle sampler = scene.getDataSlot(consumerSlot).attachedTextureSampler;
        const TextureLinkManager& linkManager = scenes.getSceneLinksManager().getTextureLinkManager();
        if (linkManager.hasLinkedOffscreenBuffer(consumerScene, sampler))
        {
            return linkManager.getLinkedOffscreenBuffer(consumerScene, sampler);
        }

        return OffscreenBufferHandle::Invalid();
    }

    void RendererLogger::LogEmbeddedCompositor(const RendererSceneUpdater& updater, RendererLogContext& context)
    {
        StartSection("EMBEDDED COMPOSITOR", context);

        context << updater.m_displayResourceManagers.count() << " known Display(s)" << RendererLogContext::NewLine << RendererLogContext::NewLine;

        context.indent();
        for(const auto& displayIt : updater.m_renderer.m_displays)
        {
            const DisplayHandle displayHandle = displayIt.first;
            const WaylandIviSurfaceId iviSurfaceId = displayIt.second.displayController->getRenderBackend().getSurface().getWindow().getWaylandIviSurfaceID();

            context << "Display [id: " << displayHandle << "; IviSurfaceId: " << iviSurfaceId.getValue() << "]" << RendererLogContext::NewLine << RendererLogContext::NewLine;

            context.indent();
            const IEmbeddedCompositor& compositor = displayIt.second.displayController->getRenderBackend().getEmbeddedCompositor();
            compositor.logInfos(context);

            context.unindent();
        }
        context.unindent();

        EndSection("EMBEDDED COMPOSITOR", context);
    }

    void RendererLogger::LogEventQueue(const RendererSceneUpdater& updater, RendererLogContext& context)
    {
        StartSection("RENDERER EVENTS", context);

        Vector<RendererEvent> events;
        updater.m_rendererEventCollector.getEvents(events);

        context << events.size() << " renderer event(s) pending to be dispatched" << RendererLogContext::NewLine;
        context.indent();
        if (events.size() > 0u)
        {
            context << "Dispatch events using RamsesRenderer::dispatchEvents(...)!" << RendererLogContext::NewLine;
        }
        context << RendererLogContext::NewLine;

        if (context.isLogLevelFlagEnabled(ERendererLogLevelFlag_Details))
        {
            UInt32 i = 0;
            for(const auto& eventIt : events)
            {
                context << ++i << ". " << EnumToString(eventIt.eventType) << RendererLogContext::NewLine;
            }
        }

        context.unindent();
        EndSection("RENDERER EVENTS", context);
    }

    void RendererLogger::LogPeriodicInfo(const RendererSceneUpdater& updater)
    {
        LOG_INFO_F(CONTEXT_PERIODIC, ([&](StringOutputStream& sos) {
                    sos.reserve(2048);

                    SceneIdVector knownSceneIds;
                    updater.m_sceneStateExecutor.m_scenesStateInfo.getKnownSceneIds(knownSceneIds);
                    sos << "Renderer: " << knownSceneIds.size() << " scene(s):";
                    Bool first = true;
                    for(const auto& sceneId : knownSceneIds)
                    {
                        if (first)
                        {
                            first = false;
                        }
                        else
                        {
                            sos << ",";
                        }
                        const ESceneState sceneState = updater.m_sceneStateExecutor.getSceneState(sceneId);
                        sos << " " << sceneId.getValue() << " " << EnumToString(sceneState);
                        const auto it = updater.m_pendingSceneActions.find(sceneId);
                        if (it != updater.m_pendingSceneActions.end())
                        {
                            const auto& pendingFlushes = it->second;
                            if (!pendingFlushes.empty())
                                sos << " (" << pendingFlushes.size() << " pending flushes)";
                        }
                    }

                    sos << "\n";
                    updater.m_renderer.getStatistics().writeStatsToStream(sos);
                    sos << "\nTime budgets:"
                        << " flushApply " << Int64(updater.m_frameTimer.getTimeBudgetForSection(EFrameTimerSectionBudget::SceneActionsApply).count()) << "us"
                        << " resourceUpload " << Int64(updater.m_frameTimer.getTimeBudgetForSection(EFrameTimerSectionBudget::ClientResourcesUpload).count()) << "us"
                        << " obRender " << Int64(updater.m_frameTimer.getTimeBudgetForSection(EFrameTimerSectionBudget::OffscreenBufferRender).count()) << "us";
                    sos << "\n";
                    updater.m_renderer.getProfilerStatistics().writeLongestFrameTimingsToStream(sos);
                    sos << "\n";
                    updater.m_renderer.getMemoryStatistics().writeMemoryUsageSummaryToString(sos);
                }));

        updater.m_renderer.getStatistics().reset();
        updater.m_renderer.getProfilerStatistics().resetFrameTimings();
        updater.m_renderer.getMemoryStatistics().reset();

        LOG_TRACE_F(CONTEXT_PERIODIC, ([&](StringOutputStream& sos)
        {
            sos << "RndClientRes states:";
            for (const auto& disp : updater.m_displayResourceManagers)
            {
                sos << " Disp" << disp.key.asMemoryHandle() << ":";
                const RendererResourceManager& resourceManager = static_cast<const RendererResourceManager&>(*disp.value);
                for (auto& seq : resourceManager.m_clientResourceRegistry.m_stateChangeSequences)
                {
                    if (seq.value.front() != '\0')
                    {
                        sos << " " << StringUtils::HexFromResourceContentHash(seq.key) << ":";
                        if (seq.value.back() != '\0')
                            sos << String(seq.value.data(), 0, seq.value.size() - 1) + "..."; // seq buffer is full, there are missing state changes
                        else
                            sos << seq.value.data();
                        seq.value.fill('\0');
                    }
                }
            }
        }));
    }
}
