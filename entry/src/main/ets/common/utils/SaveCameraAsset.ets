/*
 * Copyright (C) 2024 Huawei Device Co., Ltd.
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

import { fileIo, picker } from '@kit.CoreFileKit';
import { BusinessError } from '@kit.BasicServicesKit';
import DateTimeUtil from './DateTimeUtils';
import Logger from './Logger';

const TAG: string = 'SaveAsset';

export default class SaveAsset {
  private tag: string;
  private lastSaveTime: string = '';
  private saveIndex: number = 0;

  constructor(tag: string) {
    this.tag = tag;
  }

  public async createVideoFd(): Promise<number> {
    Logger.info(TAG, 'get Recorder File Fd');
    const mDateTimeUtil = new DateTimeUtil();
    const displayName = this.checkName(`AVCodec_${mDateTimeUtil.getDate()}_${mDateTimeUtil.getTime()}`) + '.mp4';
    Logger.info(TAG, 'get Recorder display name is: ' + displayName);
    let photoSaveOptions = new picker.PhotoSaveOptions();
    photoSaveOptions.newFileNames = [displayName];
    let photoPicker = new picker.PhotoViewPicker();
    let result = await photoPicker.save(photoSaveOptions);

    let videoFd: number = 0;
    await fileIo.open(result[0], fileIo.OpenMode.READ_WRITE | fileIo.OpenMode.CREATE).then((file) => {
      videoFd = file.fd;
      Logger.info(TAG, 'getRawFileDescriptor success fileName: ' + result[0] + ', fd: ' + videoFd);
    }).catch((err: BusinessError) => {
      Logger.error(TAG, 'open file failed with error message: ' + err.message + ', error code: ' + err.code);
    })

    Logger.info(TAG, 'leave get Recorder File Fd');
    return videoFd;
  }

  private checkName(name: string): string {
    if (this.lastSaveTime == name) {
      this.saveIndex += 1;
      return `${name}_${this.saveIndex}`;
    }
    this.lastSaveTime = name;
    this.saveIndex = 0;
    Logger.info(this.tag, 'get Recorder File name is: ' + name);
    return name;
  }
}