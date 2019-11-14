#include <gtest/gtest.h>

#define private public

#include "alias.h"
#include "mm.h"
#include "group.h"
#include "graph.h"

#include "unittest_helpers.h"

#include "feature.h"


using namespace Eigen;
using namespace xivo;

class OOSJacobiansTest : public::testing::Test {
  protected:
    void SetUp() override {

        // Create feature object and testing parameters
        MemoryManager::Create(256, 128);
        auto cfg_ = LoadJson("cfg/phab.json");
        Camera::Create(cfg_["camera_cfg"]);
        delta = 1e-6;
        tol = 1e-6;

        // IMU measurement
        gyro = Vec3::Random();

        // Set nominal and error variables to random values
        Rr_nom = RandomTransformationMatrix();
        Tr_nom = Vec3::Random();
        Rsb_nom = RandomTransformationMatrix();
        Tsb_nom = Vec3::Random();
        Rbc_nom = RandomTransformationMatrix();
        Tbc_nom = Vec3::Random();

        Wsb_err = Vec3::Zero();
        Tsb_err = Vec3::Zero();
        Wbc_err = Vec3::Zero();
        Tbc_err = Vec3::Zero();
        Xs_err = Vec3::Zero();

        // Set reference Rr and Tr for the feature
        Vec2 xp(25, 46);
        f = Feature::Create(xp(0), xp(1));

        // For OOS update, unproject to get proper value of x_
        Vec2 xc = Camera::instance()->UnProject(xp);
        f->x_(0) = xc(0);
        f->x_(1) = xc(1);
        //f->x_(2) = 2.0;
        group = Group::Create(SO3(Rr_nom), Tr_nom);
        group->sind_ = 0;
        f->ref_ = group;

        // Compute nominal Xc, Xs, Xcn
        ComputeNominalStates();

        // Construct the observation for the OOS Jacobian
        Observation obs;
        obs.g = group;
        obs.xp = xp;

        // Compute nominal Jacobian
        f->ComputeOOSJacobianInternal(obs, Rbc_nom, Tbc_nom);
    }

    Vec3 ComputeXcn() {
        Mat3 I3 = Mat3::Identity();
        Rsb = Rsb_nom*(I3 + hat(Wsb_err));
        Tsb = Tsb_nom + Tsb_err;
        Rbc = Rbc_nom*(I3 + hat(Wbc_err));
        Tbc = Tbc_nom + Tbc_err;

        Vec3 Xs = f->Xs(SE3{SO3{Rbc}, Tbc}) + Xs_err;

        Mat3 Rbc_t = Rbc.transpose();
        Mat3 Rsb_t = Rsb.transpose();
        Vec3 Xcn = Rbc_t*(Rsb_t*(Xs - Tsb) - Tbc);

        return Xcn;
    }

    void ComputeNominalStates() {
        Mat3 Rsb_nom_t = Rsb_nom.transpose();
        Mat3 Rbc_nom_t = Rbc_nom.transpose();

        Xc_nom = f->Xc(nullptr);
        Xs_nom = Rr_nom * (Rbc_nom * Xc_nom + Tbc_nom) + Tr_nom;
        Xcn_nom = Rbc_nom_t*(Rsb_nom_t*(Xs_nom - Tsb_nom) - Tbc_nom);
    }

    // Feature Object and Memory Manager
    MemoryManagerPtr mm;
    CameraPtr cam;
    GroupPtr group;
    FeaturePtr f;

    // Fake IMU measurement
    Vec3 gyro;

    // numerical tolerance
    number_t tol;

    // Real values (= nominal + error)
    Mat3 Rsb;
    Vec3 Tsb;
    Mat3 Rbc;
    Vec3 Tbc;

    // Nominal state variables containing placeholder values
    Mat3 Rr_nom;
    Vec3 Tr_nom;
    Mat3 Rsb_nom;
    Vec3 Tsb_nom;
    Mat3 Rbc_nom;
    Vec3 Tbc_nom;

    // Error variables containing placeholder values
    Vec3 Wsb_err;
    Vec3 Tsb_err;
    Vec3 Wbc_err;
    Vec3 Tbc_err;
    Vec3 Xs_err;

    // finite difference
    number_t delta;

    // Values to save for debugging
    Vec3 Xc_nom;
    Vec3 Xs_nom;
    Vec3 Xcn_nom;
};



TEST_F(OOSJacobiansTest, Wsb) {
    Vec3 Xcn0 = ComputeXcn();

    Wsb_err(0) = delta;
    Vec3 Xcn1_0 = ComputeXcn();
    Wsb_err(0) = 0;

    Wsb_err(1) = delta;
    Vec3 Xcn1_1 = ComputeXcn();
    Wsb_err(1) = 0;

    Wsb_err(2) = delta;
    Vec3 Xcn1_2 = ComputeXcn();
    Wsb_err(2) = 0;

    Vec3 dXcn_dWsb0 = (Xcn1_0 - Xcn0) / delta;
    Vec3 dXcn_dWsb1 = (Xcn1_1 - Xcn0) / delta;
    Vec3 dXcn_dWsb2 = (Xcn1_2 - Xcn0) / delta;

    EXPECT_NEAR(dXcn_dWsb0(0), f->cache_.dXcn_dWsb(0,0), tol);
    EXPECT_NEAR(dXcn_dWsb0(1), f->cache_.dXcn_dWsb(1,0), tol);
    EXPECT_NEAR(dXcn_dWsb0(2), f->cache_.dXcn_dWsb(2,0), tol);
    
    EXPECT_NEAR(dXcn_dWsb1(0), f->cache_.dXcn_dWsb(0,1), tol);
    EXPECT_NEAR(dXcn_dWsb1(1), f->cache_.dXcn_dWsb(1,1), tol);
    EXPECT_NEAR(dXcn_dWsb1(2), f->cache_.dXcn_dWsb(2,1), tol);

    EXPECT_NEAR(dXcn_dWsb2(0), f->cache_.dXcn_dWsb(0,2), tol);
    EXPECT_NEAR(dXcn_dWsb2(1), f->cache_.dXcn_dWsb(1,2), tol);
    EXPECT_NEAR(dXcn_dWsb2(2), f->cache_.dXcn_dWsb(2,2), tol);
}



TEST_F(OOSJacobiansTest, Tsb) {
    Vec3 Xcn0 = ComputeXcn();

    Tsb_err(0) = delta;
    Vec3 Xcn1_0 = ComputeXcn();
    Tsb_err(0) = 0;

    Tsb_err(1) = delta;
    Vec3 Xcn1_1 = ComputeXcn();
    Tsb_err(1) = 0;

    Tsb_err(2) = delta;
    Vec3 Xcn1_2 = ComputeXcn();
    Tsb_err(2) = 0;

    Vec3 dXcn_dTsb0 = (Xcn1_0 - Xcn0) / delta;
    Vec3 dXcn_dTsb1 = (Xcn1_1 - Xcn0) / delta;
    Vec3 dXcn_dTsb2 = (Xcn1_2 - Xcn0) / delta;

    EXPECT_NEAR(dXcn_dTsb0(0), f->cache_.dXcn_dTsb(0,0), tol);
    EXPECT_NEAR(dXcn_dTsb0(1), f->cache_.dXcn_dTsb(1,0), tol);
    EXPECT_NEAR(dXcn_dTsb0(2), f->cache_.dXcn_dTsb(2,0), tol);
    
    EXPECT_NEAR(dXcn_dTsb1(0), f->cache_.dXcn_dTsb(0,1), tol);
    EXPECT_NEAR(dXcn_dTsb1(1), f->cache_.dXcn_dTsb(1,1), tol);
    EXPECT_NEAR(dXcn_dTsb1(2), f->cache_.dXcn_dTsb(2,1), tol);

    EXPECT_NEAR(dXcn_dTsb2(0), f->cache_.dXcn_dTsb(0,2), tol);
    EXPECT_NEAR(dXcn_dTsb2(1), f->cache_.dXcn_dTsb(1,2), tol);
    EXPECT_NEAR(dXcn_dTsb2(2), f->cache_.dXcn_dTsb(2,2), tol);

}


TEST_F(OOSJacobiansTest, Wbc) {
    Vec3 Xcn0 = ComputeXcn();

    Wbc_err(0) = delta;
    Vec3 Xcn1_0 = ComputeXcn();
    Wbc_err(0) = 0;

    Wbc_err(1) = delta;
    Vec3 Xcn1_1 = ComputeXcn();
    Wbc_err(1) = 0;

    Wbc_err(2) = delta;
    Vec3 Xcn1_2 = ComputeXcn();
    Wbc_err(2) = 0;

    Vec3 dXcn_dWbc0 = (Xcn1_0 - Xcn0) / delta;
    Vec3 dXcn_dWbc1 = (Xcn1_1 - Xcn0) / delta;
    Vec3 dXcn_dWbc2 = (Xcn1_2 - Xcn0) / delta;

    EXPECT_NEAR(dXcn_dWbc0(0), f->cache_.dXcn_dWbc(0,0), tol);
    EXPECT_NEAR(dXcn_dWbc0(1), f->cache_.dXcn_dWbc(1,0), tol);
    EXPECT_NEAR(dXcn_dWbc0(2), f->cache_.dXcn_dWbc(2,0), tol);
    
    EXPECT_NEAR(dXcn_dWbc1(0), f->cache_.dXcn_dWbc(0,1), tol);
    EXPECT_NEAR(dXcn_dWbc1(1), f->cache_.dXcn_dWbc(1,1), tol);
    EXPECT_NEAR(dXcn_dWbc1(2), f->cache_.dXcn_dWbc(2,1), tol);

    EXPECT_NEAR(dXcn_dWbc2(0), f->cache_.dXcn_dWbc(0,2), tol);
    EXPECT_NEAR(dXcn_dWbc2(1), f->cache_.dXcn_dWbc(1,2), tol);
    EXPECT_NEAR(dXcn_dWbc2(2), f->cache_.dXcn_dWbc(2,2), tol);
}


TEST_F(OOSJacobiansTest, Tbc) {
    Vec3 Xcn0 = ComputeXcn();

    Tbc_err(0) = delta;
    Vec3 Xcn1_0 = ComputeXcn();
    Tbc_err(0) = 0;

    Tbc_err(1) = delta;
    Vec3 Xcn1_1 = ComputeXcn();
    Tbc_err(1) = 0;

    Tbc_err(2) = delta;
    Vec3 Xcn1_2 = ComputeXcn();
    Tbc_err(2) = 0;

    Vec3 dXcn_dTbc0 = (Xcn1_0 - Xcn0) / delta;
    Vec3 dXcn_dTbc1 = (Xcn1_1 - Xcn0) / delta;
    Vec3 dXcn_dTbc2 = (Xcn1_2 - Xcn0) / delta;

    EXPECT_NEAR(dXcn_dTbc0(0), f->cache_.dXcn_dTbc(0,0), tol);
    EXPECT_NEAR(dXcn_dTbc0(1), f->cache_.dXcn_dTbc(1,0), tol);
    EXPECT_NEAR(dXcn_dTbc0(2), f->cache_.dXcn_dTbc(2,0), tol);
    
    EXPECT_NEAR(dXcn_dTbc1(0), f->cache_.dXcn_dTbc(0,1), tol);
    EXPECT_NEAR(dXcn_dTbc1(1), f->cache_.dXcn_dTbc(1,1), tol);
    EXPECT_NEAR(dXcn_dTbc1(2), f->cache_.dXcn_dTbc(2,1), tol);

    EXPECT_NEAR(dXcn_dTbc2(0), f->cache_.dXcn_dTbc(0,2), tol);
    EXPECT_NEAR(dXcn_dTbc2(1), f->cache_.dXcn_dTbc(1,2), tol);
    EXPECT_NEAR(dXcn_dTbc2(2), f->cache_.dXcn_dTbc(2,2), tol);
}


TEST_F(OOSJacobiansTest, Xs) {
    Vec3 Xcn0 = ComputeXcn();

    Xs_err(0) = delta;
    Vec3 Xcn1_0 = ComputeXcn();
    Xs_err(0) = 0;

    Xs_err(1) = delta;
    Vec3 Xcn1_1 = ComputeXcn();
    Xs_err(1) = 0;

    Xs_err(2) = delta;
    Vec3 Xcn1_2 = ComputeXcn();
    Xs_err(2) = 0;

    Vec3 dXcn_dXs0 = (Xcn1_0 - Xcn0) / delta;
    Vec3 dXcn_dXs1 = (Xcn1_1 - Xcn0) / delta;
    Vec3 dXcn_dXs2 = (Xcn1_2 - Xcn0) / delta;

    EXPECT_NEAR(dXcn_dXs0(0), f->cache_.dXcn_dXs(0,0), tol);
    EXPECT_NEAR(dXcn_dXs0(1), f->cache_.dXcn_dXs(1,0), tol);
    EXPECT_NEAR(dXcn_dXs0(2), f->cache_.dXcn_dXs(2,0), tol);
    
    EXPECT_NEAR(dXcn_dXs1(0), f->cache_.dXcn_dXs(0,1), tol);
    EXPECT_NEAR(dXcn_dXs1(1), f->cache_.dXcn_dXs(1,1), tol);
    EXPECT_NEAR(dXcn_dXs1(2), f->cache_.dXcn_dXs(2,1), tol);

    EXPECT_NEAR(dXcn_dXs2(0), f->cache_.dXcn_dXs(0,2), tol);
    EXPECT_NEAR(dXcn_dXs2(1), f->cache_.dXcn_dXs(1,2), tol);
    EXPECT_NEAR(dXcn_dXs2(2), f->cache_.dXcn_dXs(2,2), tol);
   
}