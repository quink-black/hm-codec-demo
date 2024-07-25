/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import { camera } from '@kit.CameraKit';
import { fileIo } from '@kit.CoreFileKit';
import { display, router } from '@kit.ArkUI';
import { BusinessError } from '@kit.BasicServicesKit';
import recorder from 'librecorder.so';
import Logger from '../common/utils/Logger';
import { dateTime } from '../common/utils/DateTimeUtils';
import { CommonConstants as Const } from '../common/CommonConstants';

const TAG: string = Const.RECORDER_TAG;

const params: RouTmp = router.getParams() as RouTmp;
const encoderSurfaceId: string = params.surfaceId;
const cameraWidth: number = params.cameraWidth;
const cameraHeight: number = params.cameraHeight;
const isHDRVivid: number = params.isHDRVivid;
const frameRate: number = params.frameRate;
const outputfd: number = params.outputfd;

let cameraInput: camera.CameraInput;
let XComponentPreviewOutput: camera.PreviewOutput;
let encoderVideoOutput: camera.VideoOutput;
let videoSession: camera.VideoSession;

class RouTmp {
  surfaceId: string = Const.DEFAULT_ID;
  cameraWidth: number = Const.DEFAULT_VALUE;
  cameraHeight: number = Const.DEFAULT_VALUE;
  isHDRVivid: number = Const.DEFAULT_VALUE;
  frameRate: number = Const.DEFAULT_VALUE;
  outputfd: number = Const.DEFAULT_VALUE;
}

async function releaseCamera() {
  // Stop the video output stream
  if (encoderVideoOutput) {
    encoderVideoOutput.stop((err: BusinessError) => {
      if (err) {
        Logger.error(TAG, `Failed to stop the encoder video output. error: ${JSON.stringify(err)}`);
        return;
      }
      Logger.info(TAG, 'Callback invoked to indicate the encoder video output stop success.');
    })
  }
  // Stop the Session.
  videoSession.stop();
  // Close file fd.
  fileIo.close(outputfd);
  // Close camera input stream.
  cameraInput.close();
  // Release preview output stream.
  XComponentPreviewOutput.release();
  // Release the video output stream.
  encoderVideoOutput.release();
  Logger.info(TAG, 'encoderVideoOutput release');
  // Release session.
  videoSession.release();
}

function getCameraDevices(cameraManager: camera.CameraManager): Array<camera.CameraDevice> {
  let cameraArray: Array<camera.CameraDevice> = cameraManager.getSupportedCameras();
  if (cameraArray !== undefined && cameraArray.length <= 0) {
    Logger.error(TAG, 'cameraManager.getSupportedCameras error');
    return [];
  }
  for (let index = 0; index < cameraArray.length; index++) {
    Logger.info(TAG, 'getCameraDevices -- cameraId :' + cameraArray[index].cameraId);
    Logger.info(TAG, 'getCameraDevices -- cameraPosition :' + cameraArray[index].cameraPosition);
  }
  return cameraArray;
}

@Entry
@Component
struct Recorder {
  @State isRecorderTimeTextHide: boolean = true;
  @State playFlag: boolean = false;
  @State buttonEnabled: boolean = true;
  @State videoRecorderTimeText: string = Const.DEFAULT_TIME;
  @State fov: number = 1;
  private XComponentSurfaceId: string = Const.DEFAULT_ID;
  private cameraWidth: number = Const.DEFAULT_WIDTH;
  private cameraHeight: number = Const.DEFAULT_HEIGHT;
  private XComponentController: XComponentController = new XComponentController();
  private display = display.getDefaultDisplaySync();
  private heightPx = (this.display.width * this.cameraWidth / this.cameraHeight) + Const.PX;
  private timer: number = Const.DEFAULT_VALUE;
  private seconds: number = Const.DEFAULT_VALUE;
  private isReleased: boolean = false;
  private isBack: boolean = false;
  private range: number[] = [];

  onBackPress() {
    this.isBack = true;
  }

  onPageHide() {
    this.release();
    if (!this.isBack) {
      router.back();
    }
  }

  async release() {
    if (!this.isReleased) {
      this.isReleased = true;
      clearInterval(this.timer);
      this.seconds = 0;
      this.videoRecorderTimeText = Const.DEFAULT_TIME;

      recorder.stopNative().then(async (data) => {
        if (data.code == 0) {
          await releaseCamera();
          this.playFlag = false;
        }
      })
    }
  }

  getRecordTime(): void {
    this.timer = setInterval(() => {
      this.seconds += 1;
      this.videoRecorderTimeText = dateTime(this.seconds);
    }, 1000);
  }

  async createRecorder(): Promise<void> {
    releaseCamera();
    // Create the CameraManager object.
    let cameraManager = camera.getCameraManager(getContext());
    if (!cameraManager) {
      Logger.error(TAG, 'camera.getCameraManager error');
      return;
    }

    // Get supported camera devices.
    let camerasDevices: Array<camera.CameraDevice> = getCameraDevices(cameraManager);

    // Get profile object.
    let profiles: camera.CameraOutputCapability = cameraManager.getSupportedOutputCapability(camerasDevices[0],
      camera.SceneMode.NORMAL_VIDEO);
    if (!profiles) {
      Logger.error(TAG, 'cameraManager.getSupportedOutputCapability error');
      return;
    }

    // Get the preview stream profile
    let previewProfiles: Array<camera.Profile> = profiles.previewProfiles;
    if (!previewProfiles) {
      Logger.error(TAG, 'createOutput previewProfiles == null || undefined');
      return;
    }

    // Get the video stream profile.
    let videoProfiles: Array<camera.VideoProfile> = profiles.videoProfiles;
    if (!videoProfiles) {
      Logger.error(TAG, 'createOutput videoProfiles == null || undefined');
      return;
    }

    //The preview stream of XComponent.
    let XComponentPreviewProfile: camera.Profile = previewProfiles[0];
    if (XComponentPreviewProfile === undefined) {
      Logger.error(TAG, 'XComponentPreviewProfile is not found');
      return;
    }

    // The videoProfile's width and height need to be the same as the encoder's width and height.
    let videoSize: camera.Size = {
      width: cameraWidth,
      height: cameraHeight
    };

    // Matches the videoProfile selected by the user.
    let videoProfile: undefined | camera.VideoProfile = videoProfiles.find((profile: camera.VideoProfile) => {
      if (isHDRVivid) {
        if (frameRate === Const.FRAMERATE_VIDEO_30FPS) {
          return profile.size.width === videoSize.width &&
            profile.size.height === videoSize.height &&
            profile.format === camera.CameraFormat.CAMERA_FORMAT_YCBCR_P010 &&
            profile.frameRateRange.min === Const.MIN_RANGE &&
            profile.frameRateRange.max === Const.MAX_RANGE;
        } else {
          return profile.size.width === videoSize.width &&
            profile.size.height === videoSize.height &&
            profile.format === camera.CameraFormat.CAMERA_FORMAT_YCBCR_P010 &&
            profile.frameRateRange.min === frameRate &&
            profile.frameRateRange.max === frameRate;
        }
      } else {
        if (frameRate == Const.FRAMERATE_VIDEO_30FPS) {
          return profile.size.width === videoSize.width &&
            profile.size.height === videoSize.height &&
            profile.format === camera.CameraFormat.CAMERA_FORMAT_YUV_420_SP &&
            profile.frameRateRange.min ===  Const.MIN_RANGE &&
            profile.frameRateRange.max ===  Const.MAX_RANGE;
        } else {
          return profile.size.width === videoSize.width &&
            profile.size.height === videoSize.height &&
            profile.format === camera.CameraFormat.CAMERA_FORMAT_YUV_420_SP &&
            profile.frameRateRange.min === frameRate &&
            profile.frameRateRange.max === frameRate;
        }
      }
    });
    if (!videoProfile) {
      Logger.error(TAG, 'videoProfile is not found');
      return;
    }

    //Create the encoder output object
    encoderVideoOutput = cameraManager.createVideoOutput(videoProfile, encoderSurfaceId);
    if (encoderVideoOutput === undefined) {
      Logger.error(TAG, 'encoderVideoOutput is undefined');
      return;
    }
    Logger.info(TAG, 'encoderVideoOutput  success');

    // Create a preview stream output object
    XComponentPreviewOutput = cameraManager.createPreviewOutput(XComponentPreviewProfile, this.XComponentSurfaceId);
    if (XComponentPreviewOutput === undefined) {
      Logger.error(TAG, 'XComponentPreviewOutput is undefined');
      return;
    }

    // Create the cameraInput object.
    try {
      cameraInput = cameraManager.createCameraInput(camerasDevices[0]);
    } catch (error) {
      let err = error as BusinessError;
      Logger.error(TAG, `Failed to createCameraInput. error: ${JSON.stringify(err)}`);
    }
    if (cameraInput === undefined) {
      Logger.error(TAG, 'cameraInput is undefined');
      return;
    }

    // Turn on the camera.
    try {
      await cameraInput.open();
    } catch (error) {
      let err = error as BusinessError;
      Logger.error(TAG, `Failed to open cameraInput. error: ${JSON.stringify(err)}`);
    }

    // Session flow.
    try {
      videoSession = cameraManager.createSession(camera.SceneMode.NORMAL_VIDEO) as camera.VideoSession;
    } catch (error) {
      let err = error as BusinessError;
      Logger.error(TAG, `Failed to create the session instance. error: ${JSON.stringify(err)}`);
    }
    if (videoSession === undefined) {
      Logger.error(TAG, 'videoSession is undefined');
      return;
    }

    // Start Configuring the session.
    try {
      videoSession.beginConfig();
    } catch (error) {
      let err = error as BusinessError;
      Logger.error(TAG, `Failed to beginConfig. error: ${JSON.stringify(err)}`);
    }

    // Add CameraInput to the session.
    try {
      videoSession.addInput(cameraInput);
    } catch (error) {
      let err = error as BusinessError;
      Logger.error(TAG, `Failed to add cameraInput. error: ${JSON.stringify(err)}`);
    }

    // Add the XComponent preview stream to the session.
    try {
      videoSession.addOutput(XComponentPreviewOutput);
    } catch (error) {
      let err = error as BusinessError;
      Logger.error(TAG, `Failed to add XcomponentPreviewOutput. error: ${JSON.stringify(err)}`);
    }

    // Add the encoder video stream to the session.
    try {
      videoSession.addOutput(encoderVideoOutput);
    } catch (error) {
      let err = error as BusinessError;
      Logger.error(TAG, `Failed to add encoderVideoOutput. error: ${JSON.stringify(err)}`);
    }

    // Submit configuration information.
    try {
      await videoSession.commitConfig();
    } catch (error) {
      let err = error as BusinessError;
      Logger.error(TAG, `videoSession commitConfig error: ${JSON.stringify(err)}`);
    }

    // Session start.
    try {
      await videoSession.start();
    } catch (error) {
      let err = error as BusinessError;
      Logger.error(TAG, `videoSession start error: ${JSON.stringify(err)}`);
    }

    // Start the video output stream
    encoderVideoOutput.start((err: BusinessError) => {
      if (err) {
        Logger.error(TAG, `Failed to start the encoder video output. error: ${JSON.stringify(err)}`);
        return;
      }
      Logger.info(TAG, 'Callback invoked to indicate the encoder video output start success.');
    })
  }

  build() {
    Stack() {
      XComponent({
        id: 'recorderXComponent',
        type: XComponentType.SURFACE,
        libraryname: '',
        controller: this.XComponentController
      })
        .onLoad(() => {
          this.XComponentSurfaceId = this.XComponentController.getXComponentSurfaceId();
          this.createRecorder();
          this.playFlag = true;
          recorder.startNative();
          this.isRecorderTimeTextHide = false;
          this.getRecordTime();
        })
        .width(Const.FULL_SIZE)
        .height(this.heightPx)
        .gesture(
          PinchGesture()
            .onActionUpdate((event: GestureEvent) => {
              if (videoSession) {
                let currentFov = this.fov * event.scale;
                if (currentFov > this.range[1]) {
                  currentFov = this.range[1];
                }
                if (currentFov < this.range[0]) {
                  currentFov = this.range[0];
                }
                videoSession.setZoomRatio(currentFov);
              }
            })
            .onActionEnd((event: GestureEvent) => {
              if (videoSession) {
                this.fov = videoSession.getZoomRatio();
              }
            })
        )

      if (!this.isRecorderTimeTextHide) {
        Text(this.videoRecorderTimeText)
          .fontFamily('HarmonyHeilTi-Light')
          .fontSize(27)
          .fontColor(Color.White)
          .align(Alignment.Bottom)
          .margin({ bottom: $r('app.float.recorder_time_margin_bottom') })
      }

      Image($r('app.media.camera_pause_video_4x'))
        .width($r('app.float.recorder_image_width'))
        .height($r('app.float.recorder_image_height'))
        .margin({ top: $r('app.float.recorder_image_margin_top') })
        .onClick(() => {
          this.buttonEnabled = false;
          this.release();
          this.isBack = true;
          router.back();
        })
        .enabled(this.buttonEnabled)
    }
    .width(Const.FULL_SIZE)
    .height(Const.FULL_SIZE)
    .backgroundColor(Color.Black)
    .expandSafeArea([SafeAreaType.SYSTEM], [SafeAreaEdge.TOP, SafeAreaEdge.BOTTOM])
  }
}