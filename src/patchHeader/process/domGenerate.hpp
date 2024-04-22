#pragma once
#include"openMVG/sfm/sfm_data.hpp"
#include<string>
#include"patchHeader/domGrid/domIO.hpp"
#include"patchHeader/process/orthographicDenseMatching.hpp"
#include"patchHeader/grid/gridConfig.hpp"
#include"patchHeader/grid/initDomByInterpolate.hpp"
#include"patchHeader/grid/initInterpolate/initDomScore.hpp"


//从一个dom管理器里面做相关的dom生成管理
class DomGenerateByManager
{
protected:
    //所谓dom的管理器
    using DomManager=openMVG::sfm::SfM_Data;

    //读取sfm_data的数据
    void readDomManager(const std::string& sfmPath,
                        DomManager& dstManager)
    {
        if (!Load(dstManager, sfmPath, openMVG::sfm::ESfM_Data::ALL)) {
          std::cerr << std::endl
            << "The input SfM_Data file \""<< sfmPath << "\" cannot be read." << std::endl;
        }
    }

    //记录配置信息
    void setConfigInfo(DomManager& manager,
                       const std::string& rangeStr, //用来表示范围的字符串
                       const std::string& interOutputFolder, //中间过程的保存位置
                       double pixelLength,//空间分辨率
                       const std::string& imageRootPath //图片的根目录

    )
    {
        //记录空间分辨率
        manager.domInfo_.pixelLength_=pixelLength;
        //记录表示范围的字符串
        manager.cloudRangeInfo_=rangeStr;
        //记录中间过程保存的位置
        manager.midProceeFolder_=interOutputFolder;
        if(imageRootPath.size())
        {
            manager.s_root_path=imageRootPath;
        }
    }

    //稀疏点云的初始载入
    void drawSparseInDOM(DomManager& manager)
    {
        manager.loadViewImgs();
        //查找关注位置的点云
        //根据点云数据与设定的gsd 获得DOM图的长和宽
        //生成初始DOM 用数据domResult_记录 默认为黑色 最后DOM大小根据是否关注某一范围大小确定
          manager.focusCloudPt();
          //判断是否需要删除中间位置
          //当算法中选择只使用中相机的时候，在这里先把中相机拍摄不到的点去除
  #ifdef USE_ONLY_MID_CAM
          manager.midCameraFilter();
  #endif
  #ifdef SAVE_EXTERNAL_PROJECT_LINE
          prepareExternalProjectLine();//准备每个DOM像素的投影直线
  #endif
          //先把已经有的稀疏点云的信息部署在dom图上
          manager.drawSparsPtAtDom();
          //打印总共载入过的图片个数
          std::cout<<"图片个数: "<<openMVG::sfm::View::getLoadedImageCount()<<std::endl;
          //看是否需要用多分辨率的形式初始化DOM网格
          if(GridConfig::useInterpolateFlag())
          {
                initDomByInterpolate(manager);
                //初始化网格的分数
                initDOMScore(manager);
          }
    }

    //生成dom的核心流程
    virtual void coreGenerateProcess(DomManager& manager)
    {
        drawSparseInDOM(manager);
        try {
            manager.denseDomLikeMvs();
        } catch (int errorFlag) {
            //这里只负责显示拿到的错误信息
            std::cerr<<errorFlag<<std::endl;
            throw errorFlag;
        }
        //2022-2-22 在算法的最后阶段重新给所有的数据赋值一个颜色
        initDomByInterpolate(manager);
        //初始化网格的分数
        initDOMScore(manager);
    }

    //保存dom相关的结果
    void saveDom(DomManager& manager,
                 const std::string& outFolder)
    {
        //保存最后的DOM结果
        manager.saveDomResult(stlplus::create_filespec(outFolder, "domResult", ".bmp"));
        //释放图片内存
        manager.releaseImgs();
        //根据dom图里面的坐标重构点云
        manager.getZAsCloud(0);
        //保存点云
        openMVG::sfm::Save(manager,
          stlplus::create_filespec(outFolder, "cloud_and_poses", ".ply"),
          openMVG::sfm::ESfM_Data::STRUCTURE);
        //保存dom的网格信息
        DomIO ioTool;
        ioTool.saveDom(manager.domInfo_,
                       stlplus::create_filespec(outFolder, "grid", ".bin"));
    }
public:
    //生成dom的流程
    void domGenerate(const std::string& sfmPath,
                     const std::string& outputPath,
                     const std::string& rangeStr,
                     double pixelLength, //空间分辨率
                     const std::string& imageRootPath //图片的根目录
    )
    {
        //读取dom管理器
        DomManager manager;
        readDomManager(sfmPath,manager);//获得了三维点云信息与相机信息
        //记录空间分辨率
        setConfigInfo(manager,rangeStr,outputPath,pixelLength,imageRootPath);
        //生成dom的核心流程
        coreGenerateProcess(manager);
        //保存dom的结果
        saveDom(manager,outputPath);
    }
};
