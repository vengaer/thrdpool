ccs = ['gcc', 'clang']

pipeline {
    agent none
    environment {
        DOCKER_IMAGE='thrdpool/build'
        ARTIFACT_DIR='artifacts'
    }
    stages {
        stage('Gitlab Pending') {
            steps {
                updateGitlabCommitStatus name: 'build', state: 'pending'
            }
        }
        stage('Docker Image') {
            agent any
            steps {
                echo '-- Docker Image --'
                sh "docker build -f Dockerfile -t ${DOCKER_IMAGE} ."
            }
        }
        stage('Build') {
            when {
                beforeAgent true
                expression {
                    return env.TYPE != 'fuzz'
                }
            }
            agent {
                docker {
                    image "${DOCKER_IMAGE}"
                }
            }
            steps {
                script {
                    ccs.each { cc ->
                        echo "-- Stating ${cc} Build --"
                        sh "CC=${cc} make -B -j\$(nproc)"
                    }
                }
            }
        }
        stage('Dynamic Test') {
            when {
                beforeAgent true
                expression {
                    return env.TYPE != 'fuzz'
                }
            }
            agent {
                docker {
                    image "${DOCKER_IMAGE}"
                }
            }
            steps {
                script {
                    ccs.each { cc ->
                        stage("Test ${cc}") {
                            for(int lvl = 0; lvl < 4; lvl++) {
                                echo "Running ${cc} O${lvl} Test 1/500"
                                sh "CC=${cc} make check O=${lvl} -B -j\$(nproc)"

                                for(int i = 0; i < 499; i++) {
                                    echo "Running ${cc} O${lvl} Test ${i + 2}/500"
                                    sh "CC=${cc} make check O=${lvl} -j\$(nproc)"
                                }
                            }
                        }
                    }
                }
            }
        }
        stage('Fetch Corpora') {
            agent {
                docker {
                    image "${DOCKER_IMAGE}"
                }
            }
            steps {
                echo 'Fetching corpora'
                sh 'mkdir -p ${ARTIFACT_DIR}/prev'
                copyArtifacts filter: "${ARTIFACT_DIR}/corpora.zip", projectName: "${JOB_NAME}", fingerprintArtifacts: true, optional: true
                script {
                    if(fileExists("${ARTIFACT_DIR}/corpora.zip")) {
                        unzip zipFile: "${ARTIFACT_DIR}/corpora.zip", dir: "${ARTIFACT_DIR}/prev"
                    }
                    else {
                        echo 'No corpora found'
                    }
                }
            }
        }
        stage('Build Fuzzer') {
            agent {
                docker {
                    image "${DOCKER_IMAGE}"
                }
            }
            steps {
                script {
                    echo "-- Building Fuzzer --"
                    sh "make fuzz"
                }
            }
        }
        stage('Merge Corpora') {
            agent {
                docker {
                    image "${DOCKER_IMAGE}"
                }
            }
            steps {
                script {
                    if(fileExists("${ARTIFACT_DIR}/prev")) {
                        echo 'Merging corpora'
                        sh "make fuzzmerge CORPORA=${ARTIFACT_DIR}/prev"
                    }
                    else {
                        echo "No corpora found"
                    }
                }
            }
        }
        stage('Fuzz') {
            agent {
                docker {
                    image "${DOCKER_IMAGE}"
                }
            }
            steps {
                script {
                    if(env.TYPE != 'fuzz') {
                        fuzztime=30
                        nfuzzruns=1
                    }
                    else {
                        nfuzzruns = 10
                        fuzztime = 240
                    }
                    for(int i = 0; i < nfuzzruns; i++) {
                        echo "Fuzzing ${i + 1}/${nfuzzruns}"
                        sh "make fuzzrun FUZZTIME=${fuzztime}"
                    }
                }
            }
        }
        stage('Gitlab Success') {
            steps {
                updateGitlabCommitStatus name: 'build', state: 'success'
            }
        }
    }
    post {
        always {
            node(null) {
                script {
                    if(fileExists("${ARTIFACT_DIR}/corpora.zip")) {
                        sh "rm ${ARTIFACT_DIR}/corpora.zip"
                    }
                    zip zipFile: "${ARTIFACT_DIR}/corpora.zip", archive: true, dir: "test/fuzz/corpora", overwrite: false
                    archiveArtifacts artifacts: "${ARTIFACT_DIR}/corpora.zip", fingerprint: true
                }
                echo '-- Removing dangling docker images --'
                sh 'docker system prune -f'

                echo '-- Cleaning up --'
                deleteDir()
            }
        }
    }
}
