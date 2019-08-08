module my_data
    implicit none
    real grid(2, 500, 250)
    integer gridIndex
    integer visualScale
end module
    
function RealToGrayscaleRGB(grey) result(res)
    real, intent(in) :: grey
    real :: greySanitized
    real greyStep1
    integer greyStep2
    integer res 
    
    greySanitized = max(-1., grey)
    greySanitized = min(1., greySanitized)
    
    greyStep1 = ((greySanitized + 1.) / 2.) * 255
    greyStep2 = greyStep1
    res = ior(ishft(ior(ishft(greyStep2, 8), greyStep2), 8), greyStep2)
    
end function

subroutine Initialize()
    !DEC$ ATTRIBUTES DLLEXPORT::Initialize
    use my_data
    implicit none
    
    integer :: x, y, j
    
    !Initialize grid
    gridIndex = 1
    do j=1,2
        do x=1,500
            do y=1,250
                grid(j, x, y) = 0
            end do
        end do
    end do
    
end subroutine Initialize

    
subroutine Redraw(buffer)
    !DEC$ ATTRIBUTES DLLEXPORT::Redraw

    use my_data
    implicit none

    integer(4) :: buffer(512, 250) ! Width = stride / 4
    integer :: x, y
    
    integer grayscaleRGB 
    integer :: RealToGrayscaleRGB

    do y=1,250
        do x=1,500
            grayscaleRGB = RealToGrayscaleRGB(grid(gridIndex, x, y))
            buffer(x, y) = grayscaleRGB
        end do
    end do

end subroutine Redraw

subroutine Update()
    !DEC$ ATTRIBUTES DLLEXPORT::Update

    use my_data
    implicit none
    
    real :: total
    real :: average
    integer :: x
    integer :: y
    integer :: otherIndex
    real :: dampen, superDampen
    integer :: l, t, r, b
    
    dampen = 0.95
    superDampen = 0.8
    
    otherIndex = 3 - gridIndex
    
    do x=1,500
        do y=1,250
            l = max(x-1, 1)
            r = min(x+1, 500)
            t = max(y-1, 1)
            b = min(y+1, 250)
            
            ! Get the average of cells
            total = &
                grid(gridIndex, x, t) + grid(gridIndex, x, b) + &
                grid(gridIndex, l, y) + grid(gridIndex, r, y)
            
            average = total / 2.
            average = average - grid(otherIndex, x, y)
            average = average * dampen
        
            grid(otherIndex, x, y) = average
        end do        
    end do
    
    gridIndex = otherIndex
    
end subroutine Update

subroutine OnMouseEvent(mx, my)
    !DEC$ ATTRIBUTES DLLEXPORT::OnMouseEvent
    
    use my_data
    implicit none
    
    integer, intent(in) :: mx
    integer, intent(in) :: my
    
    grid(gridIndex, mx, my) = 30
    
end subroutine OnMouseEvent
