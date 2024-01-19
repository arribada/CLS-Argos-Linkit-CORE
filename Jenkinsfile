pipeline {
    agent {
        dockerfile {
            args '-v ${PWD}:/usr/src/app -w /usr/src/app'
            reuseNode true
        }
    }

    stages {
        stage ('Clean Repo') {
            steps {
                sh 'git clean -fdx'
            }
        }

        stage ('Compile Bootloader') {
            steps {
                catchError(buildResult: 'FAILURE', stageResult: 'FAILURE') {
                    dir('ports/nrf52840/bootloader/gentracker_secure_bootloader/gentracker_v1.0/armgcc') {
                        sh 'make mergehex'
                    }
                    dir('ports/nrf52840/bootloader/gentracker_secure_bootloader/horizon_v4.0/armgcc') {
                        sh 'make mergehex'
                    }
                    dir('ports/nrf52840/bootloader/gentracker_secure_bootloader/rspb_adafruit_feather/armgcc') {
                        sh 'make mergehex'
                    }
                }
            }
            post { 
                success { 
                archiveArtifacts 'ports/nrf52840/bootloader/gentracker_secure_bootloader/**/armgcc/_build/*_merged.hex'
                }
            }
        }

        stage ('Compile Firmware') {
            steps {
                catchError(buildResult: 'FAILURE', stageResult: 'FAILURE') {
                    dir('ports/nrf52840/build/CORE') {
                        sh 'git show-ref --tags -d | grep ^`git rev-parse HEAD` | sed -e "s,.* refs/tags/,," -e "s/\\^{}//" > TAG_NAME'
                        sh 'cmake -DCMAKE_TOOLCHAIN_FILE=../../toolchain_arm_gcc_nrf52.cmake -DDEBUG_LEVEL=4 -DBOARD=LINKIT -DCMAKE_BUILD_TYPE=Release -DMODEL=CORE ../..'
                        sh 'make -j 4'
                        sh 'nrfutil settings generate --family NRF52840 --application LinkIt_CORE_board.hex --application-version 0 --bootloader-version 1 --bl-settings-version 2 --app-boot-validation VALIDATE_ECDSA_P256_SHA256 --sd-boot-validation VALIDATE_ECDSA_P256_SHA256 --softdevice ../../drivers/nRF5_SDK_17.0.2/components/softdevice/s140/hex/s140_nrf52_7.2.0_softdevice.hex --key-file ../../nrfutil_pkg_key.pem settings.hex'
                        sh 'mergehex -m ../../bootloader/gentracker_secure_bootloader/gentracker_v1.0/armgcc/_build/cls_bootloader_v1_linkit_merged.hex LinkIt_CORE_board.hex -o m1.hex'
                        sh 'mergehex -m m1.hex settings.hex -o LinkIt_CORE_board_merged.hex'
                        sh 'rm -f m1.hex settings.hex'
				        sh "mv LinkIt_CORE_board_dfu.zip LinkIt_CORE_board_dfu-`cat TAG_NAME`.zip"
						sh "mv LinkIt_CORE_board.elf LinkIt_CORE_board-`cat TAG_NAME`.elf"
						sh "mv LinkIt_CORE_board.hex LinkIt_CORE_board-`cat TAG_NAME`.hex"
						sh "mv LinkIt_CORE_board.img LinkIt_CORE_board-`cat TAG_NAME`.img"
						sh "mv LinkIt_CORE_board_merged.hex LinkIt_CORE_board_merged-`cat TAG_NAME`.hex"
                    }
                    dir('ports/nrf52840/build/UW') {
                        sh 'git show-ref --tags -d | grep ^`git rev-parse HEAD` | sed -e "s,.* refs/tags/,," -e "s/\\^{}//" > TAG_NAME'
                        sh 'cmake -DCMAKE_TOOLCHAIN_FILE=../../toolchain_arm_gcc_nrf52.cmake -DDEBUG_LEVEL=4 -DBOARD=LINKIT -DCMAKE_BUILD_TYPE=Release -DMODEL=UW ../..'
                        sh 'make -j 4'
                        sh 'nrfutil settings generate --family NRF52840 --application LinkIt_UW_board.hex --application-version 0 --bootloader-version 1 --bl-settings-version 2 --app-boot-validation VALIDATE_ECDSA_P256_SHA256 --sd-boot-validation VALIDATE_ECDSA_P256_SHA256 --softdevice ../../drivers/nRF5_SDK_17.0.2/components/softdevice/s140/hex/s140_nrf52_7.2.0_softdevice.hex --key-file ../../nrfutil_pkg_key.pem settings.hex'
                        sh 'mergehex -m ../../bootloader/gentracker_secure_bootloader/gentracker_v1.0/armgcc/_build/cls_bootloader_v1_linkit_merged.hex LinkIt_UW_board.hex -o m1.hex'
                        sh 'mergehex -m m1.hex settings.hex -o LinkIt_UW_board_merged.hex'
                        sh 'rm -f m1.hex settings.hex'
						sh "mv LinkIt_UW_board_dfu.zip LinkIt_UW_board_dfu-`cat TAG_NAME`.zip"
						sh "mv LinkIt_UW_board.elf LinkIt_UW_board-`cat TAG_NAME`.elf"
						sh "mv LinkIt_UW_board.hex LinkIt_UW_board-`cat TAG_NAME`.hex"
						sh "mv LinkIt_UW_board.img LinkIt_UW_board-`cat TAG_NAME`.img"
						sh "mv LinkIt_UW_board_merged.hex LinkIt_UW_board_merged-`cat TAG_NAME`.hex"
                    }
                    dir('ports/nrf52840/build/SB') {
                        sh 'git show-ref --tags -d | grep ^`git rev-parse HEAD` | sed -e "s,.* refs/tags/,," -e "s/\\^{}//" > TAG_NAME'
                        sh 'cmake -DCMAKE_TOOLCHAIN_FILE=../../toolchain_arm_gcc_nrf52.cmake -DDEBUG_LEVEL=4 -DBOARD=LINKIT -DCMAKE_BUILD_TYPE=Release -DMODEL=SB ../..'
                        sh 'make -j 4'
                        sh 'nrfutil settings generate --family NRF52840 --application LinkIt_SB_board.hex --application-version 0 --bootloader-version 1 --bl-settings-version 2 --app-boot-validation VALIDATE_ECDSA_P256_SHA256 --sd-boot-validation VALIDATE_ECDSA_P256_SHA256 --softdevice ../../drivers/nRF5_SDK_17.0.2/components/softdevice/s140/hex/s140_nrf52_7.2.0_softdevice.hex --key-file ../../nrfutil_pkg_key.pem settings.hex'
                        sh 'mergehex -m ../../bootloader/gentracker_secure_bootloader/gentracker_v1.0/armgcc/_build/cls_bootloader_v1_linkit_merged.hex LinkIt_SB_board.hex -o m1.hex'
                        sh 'mergehex -m m1.hex settings.hex -o LinkIt_SB_board_merged.hex'
                        sh 'rm -f m1.hex settings.hex'
						sh "mv LinkIt_SB_board_dfu.zip LinkIt_SB_board_dfu-`cat TAG_NAME`.zip"
						sh "mv LinkIt_SB_board.elf LinkIt_SB_board-`cat TAG_NAME`.elf"
						sh "mv LinkIt_SB_board.hex LinkIt_SB_board-`cat TAG_NAME`.hex"
						sh "mv LinkIt_SB_board.img LinkIt_SB_board-`cat TAG_NAME`.img"
						sh "mv LinkIt_SB_board_merged.hex LinkIt_SB_board_merged-`cat TAG_NAME`.hex"
                    }
                    dir('ports/nrf52840/build/HORIZON') {
                        sh 'git show-ref --tags -d | grep ^`git rev-parse HEAD` | sed -e "s,.* refs/tags/,," -e "s/\\^{}//" > TAG_NAME'
                        sh 'cmake -DCMAKE_TOOLCHAIN_FILE=../../toolchain_arm_gcc_nrf52.cmake -DDEBUG_LEVEL=4 -DBOARD=HORIZON -DCMAKE_BUILD_TYPE=Release -DMODEL=CORE ../..'
                        sh 'make -j 4'
                        sh 'nrfutil settings generate --family NRF52840 --application LinkIt_horizon_board.hex --application-version 0 --bootloader-version 1 --bl-settings-version 2 --app-boot-validation VALIDATE_ECDSA_P256_SHA256 --sd-boot-validation VALIDATE_ECDSA_P256_SHA256 --softdevice ../../drivers/nRF5_SDK_17.0.2/components/softdevice/s140/hex/s140_nrf52_7.2.0_softdevice.hex --key-file ../../nrfutil_pkg_key.pem settings.hex'
                        sh 'mergehex -m ../../bootloader/gentracker_secure_bootloader/horizon_v4.0/armgcc/_build/horizon_bootloader_v4_linkit_merged.hex LinkIt_horizon_board.hex -o m1.hex'
                        sh 'mergehex -m m1.hex settings.hex -o LinkIt_horizon_board_merged.hex'
                        sh 'rm -f m1.hex settings.hex'
						sh "mv LinkIt_horizon_board_dfu.zip LinkIt_HORIZON_board_dfu-`cat TAG_NAME`.zip"
						sh "mv LinkIt_horizon_board.elf LinkIt_HORIZON_board-`cat TAG_NAME`.elf"
						sh "mv LinkIt_horizon_board.hex LinkIt_HORIZON_board-`cat TAG_NAME`.hex"
						sh "mv LinkIt_horizon_board.img LinkIt_HORIZON_board-`cat TAG_NAME`.img"
						sh "mv LinkIt_horizon_board_merged.hex LinkIt_HORIZON_board_merged-`cat TAG_NAME`.hex"
                    }
                    dir('ports/nrf52840/build/RSPB_ADAFRUIT') {
                        sh 'git show-ref --tags -d | grep ^`git rev-parse HEAD` | sed -e "s,.* refs/tags/,," -e "s/\\^{}//" > TAG_NAME'
                        sh 'cmake -DCMAKE_TOOLCHAIN_FILE=../../toolchain_arm_gcc_nrf52.cmake -DDEBUG_LEVEL=4 -DBOARD=RSPB_ADAFRUIT -DCMAKE_BUILD_TYPE=Release -DMODEL=CORE ../..'
                        sh 'make -j 4'
                        sh 'nrfutil settings generate --family NRF52840 --application LinkIt_rspb_adafruit_board.hex --application-version 0 --bootloader-version 1 --bl-settings-version 2 --app-boot-validation VALIDATE_ECDSA_P256_SHA256 --sd-boot-validation VALIDATE_ECDSA_P256_SHA256 --softdevice ../../drivers/nRF5_SDK_17.0.2/components/softdevice/s140/hex/s140_nrf52_7.2.0_softdevice.hex --key-file ../../nrfutil_pkg_key.pem settings.hex'
                        sh 'mergehex -m ../../bootloader/gentracker_secure_bootloader/rspb_adafruit_feather/armgcc/_build/rspb_bootloader_v1_linkit_merged.hex LinkIt_rspb_adafruit_board.hex -o m1.hex'
                        sh 'mergehex -m m1.hex settings.hex -o LinkIt_rspb_adafruit_board_merged.hex'
                        sh 'rm -f m1.hex settings.hex'
						sh "mv LinkIt_rspb_adafruit_board_dfu.zip LinkIt_RSPB_ADAFRUIT_board_dfu-`cat TAG_NAME`.zip"
						sh "mv LinkIt_rspb_adafruit_board.elf LinkIt_RSPB_ADAFRUIT_board-`cat TAG_NAME`.elf"
						sh "mv LinkIt_rspb_adafruit_board.hex LinkIt_RSPB_ADAFRUIT_board-`cat TAG_NAME`.hex"
						sh "mv LinkIt_rspb_adafruit_board.img LinkIt_RSPB_ADAFRUIT_board-`cat TAG_NAME`.img"
						sh "mv LinkIt_rspb_adafruit_board_merged.hex LinkIt_RSPB_ADAFRUIT_board_merged-`cat TAG_NAME`.hex"
                    }
                }
            }
            post { 
                success { 
                archiveArtifacts 'ports/nrf52840/build/**/*.hex,ports/nrf52840/build/**/*.img,ports/nrf52840/build/**/*.zip,ports/nrf52840/build/**/*.elf'
                }
            }
        }

        stage ('Compile Unit Tests') {
            steps {
                catchError(buildResult: 'UNSTABLE', stageResult: 'FAILURE') {
                    sh 'mkdir -p tests/build'
                    dir('tests/build') {
                        sh 'cmake --no-cache -GNinja  ..'
                        sh 'ninja'
                    }
                }
            }
        }

        stage ('Run Unit Tests') {
            steps {
                catchError(buildResult: 'UNSTABLE', stageResult: 'FAILURE') {
                    dir('tests/build') {
                        sh 'ln -s ../data .'
                        sh './CLSGenTrackerTests -ojunit'
                    }
                }
            }
        }

        stage ('Generate Reports') {
            steps {
                // Publish reports from static analysis tools
                recordIssues(tools: [junitParser(pattern: '**/tests/build/*.xml')])
            }
        }
    }
}
