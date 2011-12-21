//: test to open and write a test file to the current user's home directory : ${USER_HOME_ON_HADOOP}  e.g. /user/hduser
//
//  start hadoop ${HADOOP_HOME}/bin/start-all.sh
//
//  run the executable for this test
//  ./bhdfs_test_all test_hadoop
//
//  check if any error messages come up 
//
//  check if the file is successfully created on hadoop
//  ${HADOOP_HOME}/bin/hadoop dfs -ls ${USER_HOME_ON_HADOOP}
//  ${HADOOP_HOME}/bin/hadoop dfs -cat ${USER_HOME_ON_HADOOP}/testfile.txt  --> should see Hello, World!
//
// \file
#include <testlib/testlib_test.h>
#include <vcl_iostream.h>

#include "hdfs.h"

//:
void test_hadoop()
{
  hdfsFS fs = hdfsConnect("default", 0); 
  char buffer[256];
  hdfsGetWorkingDirectory(fs, buffer, sizeof(buffer));
  vcl_cout << "working directory is: " << buffer << vcl_endl;
  vcl_string writePath = vcl_string(buffer) + "/testfile.txt"; 
  hdfsFile writeFile = hdfsOpenFile(fs, writePath.c_str(), O_WRONLY|O_CREAT, 0, 0, 0); 

  TEST("open file ", !writeFile, false);

  vcl_string buffer2 = "Hello, World!"; 
  tSize num_written_bytes = hdfsWrite(fs, writeFile, (void*)buffer2.c_str(), buffer2.length()); 

  TEST("flush buffer into the file ", !hdfsFlush(fs, writeFile), true);
  hdfsCloseFile(fs, writeFile); 

  int val = hdfsExists(fs, writePath.c_str());
  TEST("check existence of test file ", val==0, true);
}

TESTMAIN(test_hadoop);
