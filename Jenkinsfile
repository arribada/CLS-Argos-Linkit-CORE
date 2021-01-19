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

        stage ('Compile Nordic Firmware') {
            steps {
                sh 'mkdir -p ports/nrf52840/build'
                dir('ports/nrf52840/build') {
                    // Debug build
                    sh 'cmake --no-cache -GNinja -DCMAKE_TOOLCHAIN_FILE=../toolchain_arm_gcc_nrf52.cmake -DBOARD=HORIZON -DCMAKE_BUILD_TYPE=Debug ..'
                    sh 'ninja'
                }
                // Append the git commit details to the filename names
                sh '''
                    folder=ports/nrf52840/build/
                    for file in $folder*.hex $folder*.bin $folder*.map $folder*.out $folder*.zip; do
                        basename=${file%.*}
                        extension=${file##*.}
                        mv "$file" "$basename"_"$(git describe --always --tags).$extension" || true
                    done
                '''
            }
            post { 
                success { 
                archiveArtifacts 'ports/nrf52840/build/*.hex,ports/nrf52840/build/*.bin,ports/nrf52840/build/*.map,ports/nrf52840/build/*.out,ports/nrf52840/build/*.zip'
                }
            }
        }

        stage ('Compile Unit Tests') {
            steps {
                sh 'mkdir -p tests/build'
                dir('tests/build') {
                    sh 'cmake --no-cache -GNinja  ..'
                    sh 'ninja'
                }
            }
        }

        stage ('Run Unit Tests') {
            steps {
                dir('tests/build') {
                    sh 'CLSGenTrackerTests'
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