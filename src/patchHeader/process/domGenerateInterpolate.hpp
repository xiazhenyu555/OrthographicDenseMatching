#pragma once
#include"patchHeader/process/domGenerate.hpp"
#include"patchHeader/process/orthographicDenseMatching.hpp"
#include"patchHeader/domGrid/domIO.hpp"
#include"patchHeader/initDOM/mvgInitDomByExistDomByCloud.hpp"
#include<string>
#include<memory>
#include"patchHeader/initDOM/addInitBackground.hpp"
#include"patchHeader/initDOM/addMaskToDom.hpp"
#include"patchHeader/grid/interfaceByDomZ.hpp"
#include"patchHeader/grid/gridHeight/getGridHeightByIndicate.hpp"
#include"patchHeader/grid/initInterpolate/initDomScore.hpp"
#include"patchHeader/process/generateMask.hpp"
#include"patchHeader/process/generateMaskByMesh.hpp"

//把DOM的网格信息传进来，然后直接做多分辨率网格化处理
class DomGenerateInterpolate : public DomGenerateByManager
{
protected:
    using DomGrid=openMVG::sfm::DomInfo;

    //顶视稠密匹配的核心函数，独立成模块方便解耦合
    std::shared_ptr<OrthographicDenseMatching> matchingPtr_;
    //向一个已经初始化过的程序中加背景图
    std::shared_ptr<AddInitBackground> backgroundAddPtr_;
    //给DOM添加mask的操作
    std::shared_ptr<AddMaskToDOM> maskAdder_;

    //用一个 低分辨率的网格初始化一个高分辨率的网格
    void initHighGrid(DomGrid& lowGrid,DomGrid& highGrid,
                      openMVG::sfm::Landmarks& pointCloud)
    {
        //把两个DOM用网格包装一下
        InterfaceByDomZ lowInterface(lowGrid);
        InterfaceByDomZ highInterface(highGrid);
        //用低分辨率指导高分辨率生成
        HeightMakerByIndicate indicater;
        indicater.makeHeightGrid(highInterface,lowInterface,pointCloud);
    }

    //带有网格形式的点云初始化
    void initDomWithGrid(const std::string& gridPath,
                         DomManager& manager)
    {
        //先做DOM从已知点云的初始化 主要是通过这一步读图片
        //先把已经有的稀疏点云的信息部署在dom图上
        matchingPtr_->beforeIterateProcess(manager);
        //判断是否有可用的DOM网格数据
        if(gridPath.size()>0)
        {
            DomIO ioTool;
            DomGrid grid;
            ioTool.loadDom(grid,gridPath);
            //用旧的低分辨率网格初始化高分辨率
            initHighGrid(grid,manager.domInfo_,manager.structure);
            //初始化这个网格的颜色和分数
            initDOMScore(manager);
        }
        //用完之后把点云清空
        manager.structure.clear();
    }

    //核心的生成过程，因为已经有了专门的初始化过程，因此这里不需要再初始化了
    void coreGenerateProcess(DomManager& manager) override
    {
        //直接开始迭代过程就可以了
        try
        {
            matchingPtr_->iterateProcess(manager);
        }
        catch(int errorFlag)
        {
            std::cerr<<errorFlag<<std::endl;
            throw errorFlag;
        }
        //2023-2-24 在最后阶段用分数比较好的点重新初始化网格
        initDomByInterpolate(manager);
        initDOMScore(manager);
    }

public:
    //构造函数，初始化稠密匹配器
    DomGenerateInterpolate()
    {
        matchingPtr_.reset(new OrthographicDenseMatching());
        //初始化一个指针，用于向DOM里面加一个初始化的背景图
        backgroundAddPtr_.reset(new AddInitBackground());
        maskAdder_.reset(new AddMaskToDOM());
    }

    //生成dom的过程，但是允许在点云初始化之后载入一个辅助性的坐标
    void generateWithIndicate(
            const std::string& sfmPath,
             const std::string& outputPath,
             const std::string& rangeStr,
             double pixelLength, //空间分辨率
             const std::string& imageRootPath, //图片的根目录
             const std::string& gridPath, //已经生成过的DOM网格单元
            const std::string& maskPath="" //对已经生成的场景添加的mask
            )
    {
        //读取dom管理器
        DomManager manager;
        readDomManager(sfmPath,manager);//获得了三维点云信息与相机信息
        //记录空间分辨率
        setConfigInfo(manager,rangeStr,outputPath,pixelLength,imageRootPath);
        //借助网格单元辅助生成DOM
        initDomWithGrid(gridPath,manager);//这一步也很重要
        //给dom添加mask
        maskAdder_->addMask(manager.domInfo_,maskPath);
        //生成dom的核心流程
        this->coreGenerateProcess(manager);
        //保存dom的结果
        saveDom(manager,outputPath);
    }
};
