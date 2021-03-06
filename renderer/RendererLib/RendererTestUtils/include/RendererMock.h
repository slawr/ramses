//  -------------------------------------------------------------------------
//  Copyright (C) 2012 BMW Car IT GmbH
//  -------------------------------------------------------------------------
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at https://mozilla.org/MPL/2.0/.
//  -------------------------------------------------------------------------

#ifndef RAMSES_RENDERERMOCK_H
#define RAMSES_RENDERERMOCK_H

#include "renderer_common_gmock_header.h"
#include "RendererLib/Renderer.h"
#include "RendererLib/FrameTimer.h"
#include "PlatformFactoryMock.h"
#include "DisplayControllerMock.h"
#include "RenderBackendMock.h"
#include "EmbeddedCompositingManagerMock.h"

namespace ramses_internal
{
    class DisplayConfig;
    class RendererScenes;
    class SceneExpirationMonitor;

template <template<typename> class MOCK_TYPE>
struct DisplayMockInfo
{
    MOCK_TYPE< DisplayControllerMock >*                         m_displayController;
    MOCK_TYPE< ramses_internal::RenderBackendMock<MOCK_TYPE> >* m_renderBackend;
    MOCK_TYPE< EmbeddedCompositingManagerMock >*                m_embeddedCompositingManager;
};

template <template<typename> class MOCK_TYPE>
class RendererMockWithMockDisplay : public ramses_internal::Renderer
{
public:
    RendererMockWithMockDisplay(const ramses_internal::IPlatformFactory& platformFactory, const RendererScenes& rendererScenes,
        const RendererEventCollector& eventCollector, const SceneExpirationMonitor& expirationMonitor, const RendererStatistics& statistics);

    virtual void createDisplayContext(const ramses_internal::DisplayConfig& displayConfig, ramses_internal::DisplayHandle displayHandle) override;
    virtual void destroyDisplayContext(ramses_internal::DisplayHandle handle) override;

    DisplayMockInfo<MOCK_TYPE>& getDisplayMock(ramses_internal::DisplayHandle handle);

    static const FrameTimer FrameTimerInstance;

private:
    HashMap< DisplayHandle, DisplayMockInfo<MOCK_TYPE> > m_displayControllers;
};

typedef RendererMockWithMockDisplay< ::testing::NiceMock >   RendererMockWithNiceMockDisplay;
typedef RendererMockWithMockDisplay< ::testing::StrictMock > RendererMockWithStrictMockDisplay;

typedef DisplayMockInfo< ::testing::NiceMock >   DisplayNiceMockInfo;
typedef DisplayMockInfo< ::testing::StrictMock > DisplayStrictMockInfo;

template <template<typename> class MOCK_TYPE>
const FrameTimer RendererMockWithMockDisplay<MOCK_TYPE>::FrameTimerInstance;
}
#endif
