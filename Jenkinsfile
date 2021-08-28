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
                        sh "CC=${cc} make -B"
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
                            echo "Running ${cc} Test 1/1000"
                            sh "CC=${cc} make check -B"

                            for(int i = 0; i < 999; i++) {
                                echo "Running ${cc} Test ${i + 2}/1000"
                                sh "CC=${cc} make check"
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
