/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.os;

/**
 * The stable Callback interface for IUpdateEngineStable.
 */
oneway interface IUpdateEngineStableCallback {
  /**
   * Invoked when a payload is being applied and there is a status update.
   *
   * @param status_code see {@link android.os.UpdateEngine.UpdateStatusConstants}.
   * @param percentage percentage of progress of the current stage.
   * @see android.os.UpdateEngineCallback#onStatusUpdate(int, float)
   */
  void onStatusUpdate(int status_code, float percentage);

  /**
   * Invoked when a payload has finished being applied.
   *
   * @param error_code see {@link android.os.UpdateEngine.ErrorCodeConstants}
   * @see android.os.UpdateEngineCallback#onPayloadApplicationComplete(int)
   */
  void onPayloadApplicationComplete(int error_code);
}
