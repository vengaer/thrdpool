ccs = ['gcc', 'clang']

pipeline {
    agent none
    environment {
        DOCKER_IMAGE='thrdpool/build'
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
        stage('Gitlab Success') {
            steps {
                updateGitlabCommitStatus name: 'build', state: 'success'
            }
        }
    }
    post {
        always {
            node(null) {
                echo '-- Removing dangling docker images --'
                sh 'docker system prune -f'

                echo '-- Cleaning up --'
                deleteDir()
            }
        }
    }
}
