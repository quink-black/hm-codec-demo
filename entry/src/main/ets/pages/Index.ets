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

import { picker } from '@kit.CoreFileKit';
import { fileIo } from '@kit.CoreFileKit';
import { display, promptAction, router } from '@kit.ArkUI';
import player from 'libplayer.so';
import recorder from 'librecorder.so';
import Logger from '../common/utils/Logger';
import SaveAsset from '../common/utils/SaveCameraAsset';
import { CommonConstants as Const } from '../common/CommonConstants';

const TAG: string = Const.INDEX_TAG;

class DataModel {
  surfaceId: string = '';
  cameraWidth: number = Const.DEFAULT_VALUE;
  cameraHeight: number = Const.DEFAULT_VALUE;
  isHDRVivid: number = Const.DEFAULT_VALUE;
  outputfd: number | null = null;
  frameRate: number = Const.DEFAULT_VALUE;
}

@Entry
@Component
struct Player {
  @State buttonEnabled: boolean = true;
  private selectFilePath: string | null = null;
  private mSaveAsset: SaveAsset = new SaveAsset(TAG);
  private videoCodecMime: string | null = Const.MIME_VIDEO_AVC;
  private isHDRVivid: number = Const.DEFAULT_VALUE;
  private cameraWidth: number = Const.DEFAULT_WIDTH;
  private cameraHeight: number = Const.DEFAULT_HEIGHT;
  private bitRate: number = Const.BITRATE_VIDEO_20M;
  private display = display.getDefaultDisplaySync();
  private heightPx = (this.display.width * this.cameraHeight / this.cameraWidth) + Const.PX;
  private frameRate: number = Const.FRAMERATE_VIDEO_30FPS;
  private outputfd: number | null = null;

  selectFile() {
    let documentSelectOptions = new picker.DocumentSelectOptions;
    let documentViewPicker = new picker.DocumentViewPicker;
    documentViewPicker.select(documentSelectOptions)
      .then((documentSelectResult) => {
        this.selectFilePath = documentSelectResult[0];
        if (this.selectFilePath == null) {
          promptAction.showToast({
            message: $r('app.string.alert'),
            duration: Const.DURATION,
            bottom: Const.BOTTOM
          });
        } else {
          this.play();
          Logger.info(TAG, 'documentViewPicker.select to file succeed and URI is:' + this.selectFilePath);
        }
      });
  }

  async selectOutputFilePath(): Promise<void> {
    this.outputfd = await this.mSaveAsset.createVideoFd();
    if (this.outputfd !== null) {
      recorder.initNative(this.outputfd, this.videoCodecMime, this.cameraWidth,
        this.cameraHeight, this.frameRate, this.isHDRVivid, this.bitRate)
        .then((data) => {
          if (data.surfaceId !== null) {
            let paramsInfo: DataModel = {
              surfaceId: data.surfaceId,
              cameraWidth: this.cameraWidth,
              cameraHeight: this.cameraHeight,
              isHDRVivid: this.isHDRVivid,
              outputfd: this.outputfd,
              frameRate: this.frameRate
            };
            router.pushUrl({
              url: 'pages/Recorder',
              params: paramsInfo
            });
          }
        })
    } else {
      Logger.error(TAG, 'get outputfd failed');
    }
  }

  play() {
    let inputFile = fileIo.openSync(this.selectFilePath, fileIo.OpenMode.READ_ONLY);
    if (!inputFile) {
      Logger.error(TAG, 'player inputFile is null');
    }
    let inputFileState = fileIo.statSync(inputFile.fd);
    if (inputFileState.size <= 0) {
      Logger.error(TAG, 'player inputFile size is 0');
    }
    this.buttonEnabled = false;
    player.playNative(inputFile.fd, Const.DEFAULT_VALUE, inputFileState.size, () => {
      Logger.info(TAG, 'player JSCallback');
      this.buttonEnabled = true;
      fileIo.close(inputFile);
    })
  }

  @Builder
  SettingButton() {
    Button() {
      Image($r('app.media.gearshape'))
        .width($r('app.float.set_image_width'))
        .height($r('app.float.set_image_height'))
    }
    .width($r('app.float.set_button_width'))
    .height($r('app.float.set_button_height'))
    .type(ButtonType.Circle)
    .backgroundColor($r('app.color.set_button_background'))
    .margin({ right: $r('app.float.set_button_margin_right') })
    .onClick(() => {
      TextPickerDialog.show({
        defaultPickerItemHeight: Const.DEFAULT_PICKER_ITEM_HEIGHT,
        selectedTextStyle: ({
          font: ({
            size: Const.SELECTED_TEXT_STYLE_FONT_SIZE
          })
        }),
        range: Const.RECORDER_INFO,
        canLoop: false,
        alignment: DialogAlignment.Center,
        backgroundColor: Color.White,
        backgroundBlurStyle: BlurStyle.BACKGROUND_ULTRA_THICK,
        onAccept: (value: TextPickerResult) => {
          switch (value.value[0]) {
            case Const.VIDEO_MIMETYPE[0]: {
              this.videoCodecMime = Const.MIME_VIDEO_HEVC;
              this.isHDRVivid = Const.TRUE;
              break;
            }
            case Const.VIDEO_MIMETYPE[1]: {
              this.videoCodecMime = Const.MIME_VIDEO_AVC;
              this.isHDRVivid = Const.FALSE;
              break;
            }
            case Const.VIDEO_MIMETYPE[2]: {
              this.videoCodecMime = Const.MIME_VIDEO_HEVC;
              this.isHDRVivid = Const.FALSE;
              break;
            }
            default:
              break;
          }

          switch (value.value[1]) {
            case Const.VIDEO_RESOLUTION[0]: {
              this.cameraWidth = Const.VIDEO_WIDTH_4K;
              this.cameraHeight = Const.VIDEO_HEIGHT_4K;
              this.bitRate = Const.BITRATE_VIDEO_30M;
              break;
            }
            case Const.VIDEO_RESOLUTION[1]: {
              this.cameraWidth = Const.VIDEO_WIDTH_1080P;
              this.cameraHeight = Const.VIDEO_HEIGHT_1080P;
              this.bitRate = Const.BITRATE_VIDEO_20M;
              break;
            }
            case Const.VIDEO_RESOLUTION[2]: {
              this.cameraWidth = Const.VIDEO_WIDTH_720P;
              this.cameraHeight = Const.VIDEO_HEIGHT_720P;
              this.bitRate = Const.BITRATE_VIDEO_10M;
              break;
            }
            default:
              break;
          }

          switch (value.value[2]) {
            case Const.VIDEO_FRAMERATE[0]: {
              this.frameRate = Const.FRAMERATE_VIDEO_30FPS;
              break;
            }
            case Const.VIDEO_FRAMERATE[1]: {
              this.frameRate = Const.FRAMERATE_VIDEO_60FPS;
              break;
            }
            default:
              break;
          }
        }
      });
    })
  }

  @Builder
  Window() {
    Row() {
      XComponent({
        id: 'player',
        type: XComponentType.SURFACE,
        libraryname: 'player'
      })
        .height(this.heightPx)
        .width(Const.FULL_SIZE)
    }
    .alignRules({
      'top': { 'anchor': '__container__', 'align': VerticalAlign.Top },
      'left': { 'anchor': '__container__', 'align': HorizontalAlign.Start }
    })
    .margin({ top: $r('app.float.window_margin_top') })
  }

  build() {
    RelativeContainer() {
      Row() {
        this.SettingButton()
      }
      .width($r('app.string.full_width'))
      .height($r('app.float.set_row_height'))
      .margin({ top: $r('app.float.set_row_margin_top') })
      .justifyContent(FlexAlign.End)
      .alignRules({
        'top': { 'anchor': '__container__', 'align': VerticalAlign.Top },
        'left': { 'anchor': '__container__', 'align': HorizontalAlign.Start }
      })

      this.Window()

      Column() {
        Button(this.buttonEnabled ? $r('app.string.play') : $r('app.string.playing'))
          .onClick(() => {
            this.selectFile();
          })
          .size({
            width: $r('app.float.index_button_width'),
            height: $r('app.float.index_button_height')
          })
          .enabled(this.buttonEnabled)
          .margin({ bottom: $r('app.float.button_margin_bottom') })

        Button($r('app.string.record'))
          .onClick(() => {
            this.selectOutputFilePath();
          })
          .size({
            width: $r('app.float.index_button_width'),
            height: $r('app.float.index_button_height')
          })
          .enabled(this.buttonEnabled)
          .margin({ bottom: $r('app.float.last_button_margin_bottom') })
      }
      .alignRules({
        'bottom': { 'anchor': '__container__', 'align': VerticalAlign.Bottom },
        'left': { 'anchor': '__container__', 'align': HorizontalAlign.Start }
      })
      .width($r('app.string.full_width'))
      .height($r('app.float.index_column_height'))
      .justifyContent(FlexAlign.End)
    }
    .width($r('app.string.full_width'))
    .height($r('app.string.full_height'))
  }
}