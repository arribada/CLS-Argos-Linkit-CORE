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
                    // Append the git tag details
                    sh '''
                        folder=ports/nrf52840/bootloader/gentracker_secure_bootloader/gentracker_v1.0/armgcc/_build
                        for file in $folder*.hex; do
                            basename=${file%.*}
                            extension=${file##*.}
                            mv "$file" "$basename"-"$(git describe --tags --abbrev=0).$extension" || true
                        done
                    '''
                }
            }
            post { 
                success { 
                archiveArtifacts 'ports/nrf52840/bootloader/gentracker_secure_bootloader/gentracker_v1.0/armgcc/_build/*.hex'
                }
            }
        }

        stage ('Compile Firmware') {
            steps {
                catchError(buildResult: 'FAILURE', stageResult: 'FAILURE') {
                    dir('ports/nrf52840/build/core') {
                        sh 'cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=../toolchain_arm_gcc_nrf52.cmake -DDEBUG_LEVEL=4 -DBOARD=LINKIT -DCMAKE_BUILD_TYPE=Debug -DMODEL=CORE ..'
                        sh 'ninja'
                    }
                    dir('ports/nrf52840/build/UW') {
                        sh 'cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=../toolchain_arm_gcc_nrf52.cmake -DDEBUG_LEVEL=4 -DBOARD=LINKIT -DCMAKE_BUILD_TYPE=Debug -DMODEL=UW ..'
                        sh 'ninja'
                    }
                    dir('ports/nrf52840/build/SB') {
                        sh 'cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=../toolchain_arm_gcc_nrf52.cmake -DDEBUG_LEVEL=4 -DBOARD=LINKIT -DCMAKE_BUILD_TYPE=Debug -DMODEL=SB ..'
                        sh 'ninja'
                    }
                    // Append the git tag details to the filename names
                    sh '''
                        folder=ports/nrf52840/build/core
                        for file in $folder*.hex $folder*.img $folder*.out $folder*.zip; do
                            basename=${file%.*}
                            extension=${file##*.}
                            mv "$file" "$basename"-"$(git describe --tags --abbrev=0).$extension" || true
                        done

                        folder=ports/nrf52840/build/UW
                        for file in $folder*.hex $folder*.img $folder*.out $folder*.zip; do
                            basename=${file%.*}
                            extension=${file##*.}
                            mv "$file" "$basename"-"$(git describe --tags --abbrev=0).$extension" || true
                        done

                        folder=ports/nrf52840/build/SB
                        for file in $folder*.hex $folder*.img $folder*.out $folder*.zip; do
                            basename=${file%.*}
                            extension=${file##*.}
                            mv "$file" "$basename"-"$(git describe --tags --abbrev=0).$extension" || true
                        done
                    '''
                }
            }
            post { 
                success { 
                archiveArtifacts 'ports/nrf52840/build/**/*.hex,ports/nrf52840/build/**/*.img,ports/nrf52840/build/**/*.zip'
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