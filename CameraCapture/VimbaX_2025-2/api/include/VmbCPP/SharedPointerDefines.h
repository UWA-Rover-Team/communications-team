/*=============================================================================
  Copyright (C) 2025 Allied Vision Technologies.  All Rights Reserved.

  Redistribution of this file, in original or modified form, without
  prior written consent of Allied Vision Technologies is prohibited.

-------------------------------------------------------------------------------

  File:        SharedPointerDefines.h

  Description: Definition of macros for using the standard shared pointer
               (std::shared_ptr) for VmbCPP.

-------------------------------------------------------------------------------

  THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR IMPLIED
  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF TITLE,
  NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR  PURPOSE ARE
  DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
  AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=============================================================================*/

#ifndef VMBCPP_SHAREDPOINTERDEFINES_H
#define VMBCPP_SHAREDPOINTERDEFINES_H
#include <memory>
#include <type_traits>
/**
* \file SharedPointerDefines.h
* \brief Definition of macros for using the standard shared pointer (std::shared_ptr) for VmbCPP.
*/

namespace VmbCPP {

    template<class T>
    using SharedPointer = std::shared_ptr<T>;
    template<class T>
    using shared_ptr = std::shared_ptr<T>;

    /**
     * \brief The function used for assigning ownership of a raw pointer to a shared pointer object.
     *
     * \param[out]  target  the shared pointer to assign the ownership to.
     * \param[in]   rawPtr  the raw pointer \p target should receive ownership of.
     *
     * \tparam T    the type of shared pointer receiving the ownership.
     * \tparam U    the type of the raw pointer; `U*` must be assignable to `T*`
     */
    template<class T, class U, typename std::enable_if<std::is_assignable<T*&, U*>::value, int>::type = 0>
    void SP_SET(std::shared_ptr<T>& sp, U* rawPtr)
    {
        sp.reset(rawPtr);
    }

    /**
     * \brief Function for resetting a shared pointer to null.
     *
     * \param[out] target   the shared pointer to set to null
     *
     * \tparam T    type the pointer points to
     */
    template<class T>
    void SP_RESET(std::shared_ptr<T>& sp) noexcept
    {
        sp.reset();
    }

    /**
     * \brief A function used for checking, if to shared pointers point to the same object.
     *
     * \param[in]   lhs the first pointer to compare
     * \param[in]   rhs the second pointer to compare
     *
     * \tparam T    The first pointer type
     * \tparam U    The second pointer type
     *
     * \return true if and only if the pointers point to the same object.
     */
    template<class T, class U>
    bool SP_ISEQUAL(const std::shared_ptr<T>& sp1, const std::shared_ptr<U>& sp2) noexcept
    {
        return sp1 == sp2;
    }

    /**
     * \brief A function used to check a shared pointer for null
     *
     * \param[in]   sp  the shared pointer to check for null
     *
     * \tparam T    The type of pointer
     *
     * \return true if and only if the pointer points to null
     */
    template<class T>
    bool SP_ISNULL(const std::shared_ptr<T>& sp)
    {
        return nullptr == sp;
    }

    /**
     * \brief a function for accessing the raw pointer of the shared pointer.
     *
     * \param[in] sp the shared pointer to get the raw pointer from
     *
     * \tparam T    the type of the pointer
     *
     * \return a raw pointer to the object owned by \p sp
     */
    template<class T>
    T* SP_ACCESS(const std::shared_ptr<T>& sp) noexcept
    {
        return sp.get();
    }

    /**
     * \brief Convert from one shared pointer type to another using dynamic_cast.
     *
     * \param[in] sp    the shared pointer object that should be converted.
     *
     * \tparam T    The target type of the conversion
     * \tparam U    The source type of the conversion
     *
     * \return A shared pointer sharing the reference counter of \p sp, if the conversion was successful
     *         and a null pointer otherwise.
     */
    template<class T, class U>
    std::shared_ptr<T> SP_DYN_CAST(const std::shared_ptr<U>& sp) noexcept
    {
        return std::dynamic_pointer_cast<T>(sp);
    }

// These are all uses of a SharedPointer shared_ptr type alias
class BasicLockable;

/**
 * \brief An alias for a shared pointer to a BasicLockable.
 */
using BasicLockablePtr = SharedPointer<BasicLockable>;

class Camera;

/**
 * \brief An alias for a shared pointer to a Camera.
 */
using CameraPtr = SharedPointer<Camera>;

class Feature;

/**
 * \brief An alias for a shared pointer to a Feature.
 */
using FeaturePtr = SharedPointer<Feature>;

class FeatureContainer;

/**
 * \brief An alias for a shared pointer to a FeatureContainer.
 */
using FeatureContainerPtr = SharedPointer<FeatureContainer>;

class Frame;

/**
 * \brief An alias for a shared pointer to a Frame.
 */
using FramePtr = SharedPointer<Frame>;

class FrameHandler;

/**
 * \brief An alias for a shared pointer to a FrameHandler.
 */
using FrameHandlerPtr = SharedPointer<FrameHandler>;

class ICameraFactory;

/**
 * \brief An alias for a shared pointer to a camera factory.
 */
using ICameraFactoryPtr = SharedPointer<ICameraFactory>;

class ICameraListObserver;

/**
 * \brief An alias for a shared pointer to a camera list observer.
 */
using ICameraListObserverPtr = SharedPointer<ICameraListObserver>;

class IFeatureObserver;

/**
 * \brief An alias for a shared pointer to a feature observer.
 */
using IFeatureObserverPtr = SharedPointer<IFeatureObserver>;

class IFrameObserver;

/**
 * \brief An alias for a shared pointer to a frame observer.
 */
using IFrameObserverPtr = SharedPointer<IFrameObserver>;

class Interface;

/**
 * \brief An alias for a shared pointer to an Interface.
 */
using InterfacePtr = SharedPointer<Interface>;

class IInterfaceListObserver;

/**
 * \brief An alias for a shared pointer to an interface list observer.
 */
using IInterfaceListObserverPtr = SharedPointer<IInterfaceListObserver>;

class LocalDevice;

/**
 * \brief An alias for a shared pointer to a LocalDevice.
 */
using LocalDevicePtr = SharedPointer<LocalDevice>;

class Mutex;

/**
 * \brief An alias for a shared pointer to a Mutex.
 */
using MutexPtr = SharedPointer<Mutex>;

class Stream;

/**
 * \brief An alias for a shared pointer to a Stream.
 */
using StreamPtr = SharedPointer<Stream>;

class TransportLayer;

/**
 * \brief An alias for a shared pointer to a TransportLayer.
 */
using TransportLayerPtr = SharedPointer<TransportLayer>;

}  // namespace VmbCPP

#endif // VMBCPP_SHAREDPOINTERDEFINES_H
