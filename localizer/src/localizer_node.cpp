//
// Created by sumin on 22. 4. 17.
//

#include "localizer/localizer.h"
using namespace std;
int main(int argc, char** argv){
    ros::init(argc, argv, "localizer");
    Localizer loc;
    ros::spin();
    return 0;

}